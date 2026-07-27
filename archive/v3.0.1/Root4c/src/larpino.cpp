#include "larpino.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <ctime>

#define LARPINO_MAX_CTX 2048
#define LARPINO_TOP_K 40
#define LARPINO_TEMP 0.7f

#define GGUF_MAGIC 0x46554747u

enum ggml_type { GGML_TYPE_F32=0, GGML_TYPE_F16=1, GGML_TYPE_Q4_0=2 };
struct block_q4_0 { uint16_t d; uint8_t qs[16]; };

static float fp16_to_fp32(uint16_t h) {
    uint32_t s=(h>>15)&1, e=(h>>10)&0x1f, m=h&0x3ff;
    if(e==0){ int32_t v=(s<<31)|((0x7f-15)<<23)|(m<<13); float f; memcpy(&f,&v,4); return f; }
    if(e==0x1f) return m?NAN:(s?-INFINITY:INFINITY);
    int32_t v=(s<<31)|((e+0x70)<<23)|(m<<13); float f; memcpy(&f,&v,4); return f;
}
static float deq4(const block_q4_0 *b, int i){ int q=b->qs[i/2]; if(i%2==0)q&=0x0f;else q>>=4; return (q-8)*fp16_to_fp32(b->d); }

struct Tensor {
    std::string name; uint32_t type=0, n_dims=0; size_t ne[4]={1,1,1,1}, n_elems=0;
    float *f32=nullptr; block_q4_0 *q4=nullptr;
    ~Tensor(){ delete[]f32; delete[]q4; }
    Tensor(Tensor&&o) noexcept { *this=std::move(o); }
    Tensor& operator=(Tensor&&o) noexcept {
        name=std::move(o.name); type=o.type; n_dims=o.n_dims; memcpy(ne,o.ne,sizeof(ne));
        n_elems=o.n_elems; f32=o.f32; q4=o.q4; o.f32=nullptr; o.q4=nullptr; return *this;
    }
    Tensor()=default;
    Tensor(const Tensor&)=delete;
    Tensor& operator=(const Tensor&)=delete;
    float at(size_t i)const{ if(f32)return f32[i]; if(q4)return deq4(&q4[i/32],i%32); return 0; }
};

struct larpino_model {
    bool loaded=false, stop=false;
    int dim=0, n_layers=0, n_heads=0, n_kv_heads=0, vocab_size=0, hidden_dim=0;
    float norm_eps=1e-5f;
    std::unordered_map<std::string,Tensor> tensors;
    std::vector<std::string> vocab;
    std::vector<float> scores;
    std::vector<std::pair<int,int>> merges;
    std::unordered_map<std::string,int> tmap;
    float *k_cache=nullptr, *v_cache=nullptr, *buf=nullptr, *tmp=nullptr;
    int cache_len=0, kv_dim=0;
    ~larpino_model(){ free(k_cache); free(v_cache); free(buf); free(tmp); }
};

static size_t blk_size(uint32_t t, size_t &elems) {
    if(t==GGML_TYPE_Q4_0){ elems=32; return sizeof(block_q4_0); }
    elems=1; return 4;  /* F32 */
}

static void rms_norm(float *o, const float *x, int n, float eps) {
    float ss=0; for(int i=0;i<n;i++)ss+=x[i]*x[i];
    float s=1.0f/sqrtf(ss/n+eps); for(int i=0;i<n;i++)o[i]=x[i]*s;
}
static float dot(const float *a, const float *b, int n){ float s=0; for(int i=0;i<n;i++)s+=a[i]*b[i]; return s; }
static void softmax(float *a, int n){
    float mx=a[0]; for(int i=1;i<n;i++)if(a[i]>mx)mx=a[i];
    float sum=0; for(int i=0;i<n;i++){a[i]=expf(a[i]-mx);sum+=a[i];} for(int i=0;i<n;i++)a[i]/=sum;
}
static float silu(float x){ return x/(1.0f+expf(-x)); }
static void rope(float *x, int dim, int nh, int pos){
    int hd=dim/nh;
    for(int h=0;h<nh;h++){
        float *xh=x+h*hd;
        for(int i=0;i<hd;i+=2){
            float th=(float)pos*powf(10000.0f,-2.0f*i/hd), s=sinf(th), c=cosf(th);
            float x0=xh[i],x1=xh[i+1]; xh[i]=x0*c-x1*s; xh[i+1]=x0*s+x1*c;
        }
    }
}

static void matmul_q4(float *y, const float *x, const block_q4_0 *w, int m, int n, int k) {
    int bpr=(k+31)/32;
    for(int i=0;i<m;i++) for(int j=0;j<n;j++){
        float s=0; const block_q4_0 *row=w+j*bpr;
        for(int b=0;b<bpr;b++){
            float d=fp16_to_fp32(row[b].d); int base=b*32;
            for(int l=0;l<32&&base+l<k;l++){
                int q=row[b].qs[l/2]; if(l%2==0)q&=0x0f;else q>>=4;
                s+=x[i*k+base+l]*((q-8)*d);
            }
        } y[i*n+j]=s;
    }
}

static void get_row(const larpino_model *m, const char *name, int row, float *out) {
    auto it=m->tensors.find(name);
    if(it==m->tensors.end()){ memset(out,0,m->dim*4); return; }
    auto &t=it->second; int c=(int)t.ne[0];
    if(t.f32){ memcpy(out,t.f32+row*c,c*4); return; }
    if(t.q4){ int b=(c+31)/32; auto *rp=t.q4+row*b; for(int j=0;j<c;j++)out[j]=deq4(&rp[j/32],j%32); }
}

static int forward(larpino_model *m, int *tokens, int n_tokens) {
    int D=m->dim, NH=m->n_heads, NK=m->n_kv_heads, HD=D/NH, NL=m->n_layers, MX=LARPINO_MAX_CTX, KVD=m->kv_dim;
    if(KVD==0) KVD=NK*HD;
    std::vector<float> h(D), ao(D), q(D), k(D), v(D), sc(NH*MX), lg(m->vocab_size);
    auto *tmp=m->tmp;

    for(int t=0;t<n_tokens;t++){
        int pos=m->cache_len+t, tok=tokens[t];
        if(pos>=MX) break;
        if(tok>=0&&tok<m->vocab_size){
            auto &te=m->tensors["token_embd.weight"];
            if(te.f32) memcpy(h.data(),te.f32+tok*D,D*4);
            else { get_row(m,"token_embd.weight",tok,m->tmp); memcpy(h.data(),m->tmp,D*4); }
        } else { memset(h.data(),0,D*4); }

        for(int l=0;l<NL;l++){
            char b[128]; size_t lo=(size_t)l*2*MX*D;
            snprintf(b,128,"blk.%d.attn_norm.weight",l); get_row(m,b,0,m->buf);
            rms_norm(h.data(),h.data(),D,m->norm_eps); for(int i=0;i<D;i++)h[i]*=m->buf[i];

            snprintf(b,128,"blk.%d.attn_q.weight",l); matmul_q4(q.data(),h.data(),m->tensors[b].q4,1,D,D);
            snprintf(b,128,"blk.%d.attn_k.weight",l); matmul_q4(tmp,h.data(),m->tensors[b].q4,1,KVD,D);
            snprintf(b,128,"blk.%d.attn_v.weight",l); matmul_q4(v.data(),h.data(),m->tensors[b].q4,1,KVD,D);
            rope(q.data(),D,NH,pos);
            rope(tmp,KVD,NK,pos);

            memcpy(m->k_cache+lo+pos*D,tmp,(size_t)KVD*4);
            memcpy(m->v_cache+lo+MX*D+pos*D,v.data(),(size_t)KVD*4);
            memset(ao.data(),0,D*4);

            for(int hh=0;hh<NH;hh++){
                int kvh=hh%NK; float *qh=q.data()+hh*HD;
                for(int p=0;p<=pos;p++){
                    float *kh=m->k_cache+lo+p*D+kvh*HD;
                    sc[hh*MX+p]=dot(qh,kh,HD)/sqrtf((float)HD);
                }
                softmax(sc.data()+hh*MX,pos+1); float *oh=ao.data()+hh*HD;
                for(int p=0;p<=pos;p++){
                    float w=sc[hh*MX+p]; float *vh=m->v_cache+lo+MX*D+p*D+kvh*HD;
                    for(int i=0;i<HD;i++) oh[i]+=w*vh[i];
                }
            }
            snprintf(b,128,"blk.%d.attn_out.weight",l);
            { std::vector<float> tmp2(D); matmul_q4(tmp2.data(),ao.data(),m->tensors[b].q4,1,D,D); for(int i=0;i<D;i++)h[i]+=tmp2[i]; }

            snprintf(b,128,"blk.%d.ffn_norm.weight",l); get_row(m,b,0,m->buf);
            rms_norm(h.data(),h.data(),D,m->norm_eps); for(int i=0;i<D;i++)h[i]*=m->buf[i];

            snprintf(b,128,"blk.%d.ffn_gate.weight",l);
            std::vector<float> gt(m->hidden_dim);
            matmul_q4(gt.data(),h.data(),m->tensors[b].q4,1,m->hidden_dim,D);
            for(int i=0;i<m->hidden_dim;i++)gt[i]=silu(gt[i]);

            snprintf(b,128,"blk.%d.ffn_up.weight",l);
            std::vector<float> up(m->hidden_dim);
            matmul_q4(up.data(),h.data(),m->tensors[b].q4,1,m->hidden_dim,D);
            for(int i=0;i<m->hidden_dim;i++)gt[i]*=up[i];

            snprintf(b,128,"blk.%d.ffn_down.weight",l);
            std::vector<float> fo(D);
            matmul_q4(fo.data(),gt.data(),m->tensors[b].q4,1,D,m->hidden_dim);
            for(int i=0;i<D;i++)h[i]+=fo[i];
        }
        get_row(m,"output_norm.weight",0,m->buf);
        rms_norm(h.data(),h.data(),D,m->norm_eps); for(int i=0;i<D;i++)h[i]*=m->buf[i];

        auto &ow=m->tensors["output.weight"];
        if(ow.f32){ float *w=ow.f32; for(int i=0;i<m->vocab_size;i++)lg[i]=dot(h.data(),w+i*D,D); }
        else matmul_q4(lg.data(),h.data(),ow.q4,1,m->vocab_size,D);
    }
    m->cache_len+=n_tokens;

    float tp=LARPINO_TEMP;
    if(tp>0){
        for(int i=0;i<m->vocab_size;i++)lg[i]/=tp;
        softmax(lg.data(),m->vocab_size);
        std::vector<std::pair<float,int>> sv;
        for(int i=0;i<m->vocab_size;i++)sv.push_back({lg[i],i});
        std::partial_sort(sv.begin(),sv.begin()+LARPINO_TOP_K,sv.end(),[](auto&a,auto&b){return a.first>b.first;});
        float sum=0, cum=0; for(int i=0;i<LARPINO_TOP_K;i++)sum+=sv[i].first;
        float r=(float)rand()/RAND_MAX*sum;
        for(int i=0;i<LARPINO_TOP_K;i++){ cum+=sv[i].first; if(cum>=r)return sv[i].second; }
    }
    return (int)(std::max_element(lg.begin(),lg.end())-lg.begin());
}

static std::vector<int> tokenize(larpino_model *m, const char *text) {
    std::vector<int> ids;
    ids.push_back(m->tmap.count(" ") ? m->tmap[" "] : 29871);
    const char *p=text;
    while(*p){
        unsigned char uc=(unsigned char)*p;
        std::string ch(1,(char)uc);
        auto it=m->tmap.find(ch);
        ids.push_back(it!=m->tmap.end()?it->second:(int)uc);
        p++;
    }
    bool merged=true;
    while(merged){
        merged=false; int best=1000000, bi=-1;
        for(size_t i=0;i+1<ids.size();i++) for(size_t j=0;j<m->merges.size();j++)
            if(m->merges[j].first==ids[i]&&m->merges[j].second==ids[i+1])
                if((int)j<best){ best=(int)j; bi=(int)i; break; }
        if(bi>=0){
            ids[bi]=(int)(m->vocab.size()-m->merges.size()+best);
            ids.erase(ids.begin()+bi+1); merged=true;
        }
    }
    return ids;
}

static std::string detokenize(larpino_model *m, int tok) {
    return (tok>=0&&tok<(int)m->vocab.size())?m->vocab[tok]:"";
}

/* ─── On-device LLM inference engine (larpino) ───
 * Loads GGUF-format transformer models (Llama/LLaMA architecture) and
 * runs inference for text generation.  Uses Q4_0 quantization for efficiency.
 *
 * Architecture: standard Llama decoder-only transformer with RoPE, RMS norm,
 * SiLU-gated FFN, and KV-cache for autoregressive generation.
 *
 * Future work:
 *  - Support Q8_0, Q4_K_M, and other quantization formats.
 *  - Add token streaming callback for real-time display.
 *  - Add system prompt / conversation history management.
 *  - GPU acceleration via CUDA/Vulkan (compute shaders).
 *  - Speculative decoding for faster generation.
 *  - Grammar-constrained generation (GBNF-like).
 *
 * ─── Public API ───
 *   larpino_load(path)       — Load a GGUF model file.
 *   larpino_chat(m, prompt)  — Generate response to prompt.
 *   larpino_stop(m)          — Stop generation (from another thread).
 *   larpino_status(m, buf)   — Get model status string.
 *   larpino_free(m)          — Free all resources.
 *   larpino_is_loaded(m)     — Check if model is loaded.
 */
larpino_model* larpino_load(const char *path) {
    larpino_model *m=new larpino_model();
    FILE *f=fopen(path,"rb");
    if(!f){ fprintf(stderr,"larpino: can't open %s\n",path); delete m; return nullptr; }

    auto r8=[&](void *b,size_t n){ return fread(b,1,n,f)==n; };
    auto rv=[&](auto &v){ r8(&v,sizeof(v)); };
    uint32_t magic; rv(magic);
    uint32_t ver; rv(ver);
    if(magic!=GGUF_MAGIC||ver!=3){ fprintf(stderr,"larpino: bad GGUF\n"); fclose(f); delete m; return nullptr; }
    uint64_t n_tensors, n_kv; rv(n_tensors); rv(n_kv);

    for(uint64_t i=0;i<n_kv;i++){
        auto rs=[&]()->std::string{
            uint64_t ln; rv(ln); std::string s(ln,0); if(ln)r8(&s[0],ln); return s;
        };
        std::string key=rs();
        uint32_t vt; rv(vt);

        if(vt==9){  /* array */
            uint32_t at; rv(at); uint64_t al; rv(al);
            if(key=="tokenizer.ggml.tokens"){
                m->vocab.resize(al); for(uint64_t j=0;j<al;j++){m->vocab[j]=rs();m->tmap[m->vocab[j]]=(int)j;}
                m->vocab_size=(int)al; continue;
            }
            if(key=="tokenizer.ggml.scores"){ m->scores.resize(al); for(uint64_t j=0;j<al;j++)rv(m->scores[j]); continue; }
            if(key=="tokenizer.ggml.merges"){
                for(uint64_t j=0;j<al;j++){
                    std::string mg=rs(); size_t p=mg.find(' ');
                    if(p!=std::string::npos) m->merges.push_back({m->tmap[mg.substr(0,p)],m->tmap[mg.substr(p+1)]});
                } continue;
            }
            for(uint64_t j=0;j<al;j++){ if(at==8)rs(); else{ float x; rv(x); } }
            continue;
        }
        std::string val;
        if(vt==8){ val=rs(); }
        else if(vt==4||vt==5||vt==10||vt==11){ int64_t iv; rv(iv); val=std::to_string(iv); }
        else if(vt==6){ float fv; rv(fv); char b[32]; snprintf(b,32,"%g",fv); val=b; }
        else { float fv; rv(fv); val=std::to_string(fv); }

        if(key=="llama.attention.head_count") m->n_heads=atoi(val.c_str());
        else if(key=="llama.attention.head_count_kv") m->n_kv_heads=atoi(val.c_str());
        else if(key=="llama.block_count") m->n_layers=atoi(val.c_str());
        else if(key=="llama.embedding_length") m->dim=atoi(val.c_str());
        else if(key=="llama.feed_forward_length") m->hidden_dim=atoi(val.c_str());
        else if(key=="llama.norm_rms_epsilon") m->norm_eps=(float)atof(val.c_str());
    }

    if(m->n_kv_heads==0)m->n_kv_heads=m->n_heads;
    if(m->hidden_dim==0)m->hidden_dim=m->dim*4;

    struct TI{ std::string name; uint32_t nd,ty; uint64_t dm[4],off; };
    std::vector<TI> infos(n_tensors);
    for(auto &ti:infos){
        auto rs=[&]()->std::string{ uint64_t ln; rv(ln); std::string s(ln,0); if(ln)r8(&s[0],ln); return s; };
        ti.name=rs(); rv(ti.nd); rv(ti.ty);
        for(uint32_t j=0;j<ti.nd;j++)rv(ti.dm[j]);
        for(uint32_t j=ti.nd;j<4;j++)ti.dm[j]=1;
        rv(ti.off);
    }
    for(auto &ti:infos){
        Tensor t; t.name=ti.name; t.type=ti.ty; t.n_dims=ti.nd; memcpy(t.ne,ti.dm,sizeof(ti.dm));
        size_t be; size_t bs=blk_size(ti.ty,be); t.n_elems=1; for(uint32_t j=0;j<4;j++)t.n_elems*=ti.dm[j];
        size_t nb=(t.n_elems+be-1)/be, ds=nb*bs;
        fseek(f,(long)ti.off,SEEK_SET);
        if(ti.ty==GGML_TYPE_F32){ t.f32=new float[t.n_elems]; r8(t.f32,ds); }
        else if(ti.ty==GGML_TYPE_Q4_0){ t.q4=new block_q4_0[nb]; r8(t.q4,ds); }
        else if(ti.ty==GGML_TYPE_F16){
            t.f32=new float[t.n_elems]; std::vector<uint16_t> f16(t.n_elems);
            r8(f16.data(),ds); for(size_t i=0;i<t.n_elems;i++) t.f32[i]=fp16_to_fp32(f16[i]);
        }
        else continue;
        m->tensors[ti.name]=std::move(t);
    }
    fclose(f);

    /* read hparams from tensor names if not set by metadata */
    if(m->dim==0) for(auto&p:m->tensors){
        if(p.first.find("attn_q.weight")!=std::string::npos)
            m->dim=(int)p.second.ne[0];
        if(p.first.find("ffn_gate.weight")!=std::string::npos)
            m->hidden_dim=(int)p.second.ne[0];
    }
    if(m->n_layers==0) for(auto&p:m->tensors){
        if(p.first.find("blk.")==0&&p.first.find(".attn_q.weight")!=std::string::npos){
            int l=atoi(p.first.c_str()+4); if(l>=m->n_layers)m->n_layers=l+1;
        }
    }

    /* kv_dim from attn_k.weight shape */
    auto ki=m->tensors.find("blk.0.attn_k.weight");
    if(ki!=m->tensors.end()) m->kv_dim=(int)ki->second.ne[0];

    m->k_cache=(float*)calloc((size_t)m->n_layers*2*LARPINO_MAX_CTX*m->dim,4);
    m->v_cache=(float*)calloc((size_t)m->n_layers*2*LARPINO_MAX_CTX*m->dim,4);
    m->buf=(float*)calloc(m->dim,4);
    int kvd=m->kv_dim?m->kv_dim:m->dim;
    m->tmp=(float*)calloc(kvd,4);
    if(!m->k_cache||!m->v_cache||!m->buf||!m->tmp){
        fprintf(stderr,"  Out of memory loading model buffers.\n");
        if(m->k_cache)free(m->k_cache);
        if(m->v_cache)free(m->v_cache);
        if(m->buf)free(m->buf);
        if(m->tmp)free(m->tmp);
        delete m;
        return nullptr;
    }
    m->loaded=true;
    return m;
}

void larpino_free(larpino_model *m){ delete m; }
int larpino_is_loaded(const larpino_model *m){ return m&&m->loaded; }

int larpino_chat(larpino_model *m, const char *prompt, larpino_callback cb, void *user){
    if(!m||!m->loaded) return -1;
    m->stop=false; m->cache_len=0;
    srand((unsigned)time(nullptr));

    std::vector<int> toks=tokenize(m,prompt);
    if(toks.empty()) return -1;
    if((int)toks.size()>LARPINO_MAX_CTX-50) toks.resize(LARPINO_MAX_CTX-50);

    for(int g=0;g<LARPINO_MAX_CTX-(int)toks.size();g++){
        if(m->stop) break;
        int next=forward(m,toks.data(),(int)toks.size());
        if(next==2) break;
        std::string p=detokenize(m,next);
        if(!p.empty()) cb(p.c_str(),user);
        toks={next};
    }
    return 0;
}

void larpino_stop(larpino_model *m){ if(m)m->stop=true; }

const char* larpino_status(larpino_model *m, char *buf, size_t size){
    if(!m||!m->loaded) snprintf(buf,size,"no model loaded");
    else snprintf(buf,size,"loaded | dim=%d layers=%d heads=%d/%d ctx=%d/%d",
        m->dim,m->n_layers,m->n_heads,m->n_kv_heads,m->cache_len,LARPINO_MAX_CTX);
    return buf;
}
