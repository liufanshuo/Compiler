#include <bits/stdc++.h>
using namespace std;

enum {TK_EOF=0,TK_ID=256,TK_INT,TK_FLOAT,TK_STR,TK_CONST,TK_INTTK,TK_FLOATTK,TK_VOID,
      TK_IF,TK_ELSE,TK_WHILE,TK_BREAK,TK_CONTINUE,TK_RETURN,TK_LE,TK_GE,TK_EQ,TK_NE,TK_AND,TK_OR};
struct Tok{int k,line;string s;};

struct Lexer{
    string p; size_t i=0; int line=1; vector<Tok> ts;
    Lexer(const string& s):p(s){}
    char c(int o=0){return i+o<p.size()?p[i+o]:0;}
    void add(int k,string s=""){ts.push_back({k,line,s});}
    static bool id0(char c){return isalpha((unsigned char)c)||c=='_';}
    static bool idc(char c){return isalnum((unsigned char)c)||c=='_';}
    void skip(){
        for(;;){
            while(isspace((unsigned char)c())){if(c()=='\n')line++; i++;}
            if(c()=='/'&&c(1)=='/'){while(c()&&c()!='\n')i++; continue;}
            if(c()=='/'&&c(1)=='*'){i+=2; while(c()&&!(c()=='*'&&c(1)=='/')){if(c()=='\n')line++; i++;} if(c())i+=2; continue;}
            break;
        }
    }
    string scan_num(bool &isf){
        size_t b=i; isf=false;
        if(c()=='0'&&(c(1)=='x'||c(1)=='X')){
            i+=2; while(isxdigit((unsigned char)c()))i++;
            if(c()=='.'){isf=true; i++; while(isxdigit((unsigned char)c()))i++;}
            if(c()=='p'||c()=='P'){isf=true; i++; if(c()=='+'||c()=='-')i++; while(isdigit((unsigned char)c()))i++;}
            if(c()=='f'||c()=='F'||c()=='l'||c()=='L')i++;
            return p.substr(b,i-b);
        }
        if(c()=='.'){isf=true; i++; while(isdigit((unsigned char)c()))i++;}
        else {while(isdigit((unsigned char)c()))i++; if(c()=='.'){isf=true; i++; while(isdigit((unsigned char)c()))i++;}}
        if(c()=='e'||c()=='E'){isf=true; i++; if(c()=='+'||c()=='-')i++; while(isdigit((unsigned char)c()))i++;}
        if(c()=='f'||c()=='F'||c()=='l'||c()=='L')i++;
        return p.substr(b,i-b);
    }
    void run(){
        static unordered_map<string,int> kw={{"const",TK_CONST},{"int",TK_INTTK},{"float",TK_FLOATTK},{"void",TK_VOID},
        {"if",TK_IF},{"else",TK_ELSE},{"while",TK_WHILE},{"break",TK_BREAK},{"continue",TK_CONTINUE},{"return",TK_RETURN}};
        while(true){
            skip(); if(!c())break;
            if(id0(c())){size_t b=i++; while(idc(c()))i++; string s=p.substr(b,i-b); add(kw.count(s)?kw[s]:TK_ID,s); continue;}
            if(isdigit((unsigned char)c())||(c()=='.'&&isdigit((unsigned char)c(1)))){bool f; string s=scan_num(f); add(f?TK_FLOAT:TK_INT,s); continue;}
            if(c()=='"'){i++; string s; while(c()&&c()!='"'){if(c()=='\\'){s+=c(); i++; if(c())s+=c(),i++;}else{s+=c(); if(c()=='\n')line++; i++;}} if(c())i++; add(TK_STR,s); continue;}
            string two=p.substr(i,2);
            if(two=="<="){add(TK_LE);i+=2;} else if(two==">="){add(TK_GE);i+=2;} else if(two=="=="){add(TK_EQ);i+=2;}
            else if(two=="!="){add(TK_NE);i+=2;} else if(two=="&&"){add(TK_AND);i+=2;} else if(two=="||"){add(TK_OR);i+=2;}
            else add((unsigned char)c(),string(1,c())),i++;
        }
        add(TK_EOF);
    }
};

enum Base{BI,BF,BV};
struct Expr; struct Init; struct Stmt;
struct Param{Base b; string n; bool arr=false; vector<Expr*> dims;};
struct DeclItem{string n; vector<Expr*> dims; Init* init=nullptr;};
struct Decl{Base b; bool cn=false; vector<DeclItem> a;};
struct Expr{int k=0,op=0; string s; long long iv=0; float fv=0; vector<Expr*> a;};
struct Init{bool ex=false; Expr* e=nullptr; vector<Init*> ch;};
struct Stmt{int k=0; Decl* d=nullptr; Expr *e=nullptr,*e2=nullptr,*e3=nullptr; string s; vector<Expr*> es; vector<Stmt*> ss;};
struct Func{Base r; string n; vector<Param> ps; Stmt* body=nullptr;};
struct Top{Decl* d=nullptr; Func* f=nullptr;};

Expr* ne(int k){auto x=new Expr();x->k=k;return x;}
Stmt* ns(int k){auto x=new Stmt();x->k=k;return x;}

struct Parser{
    vector<Tok> t; size_t p=0;
    Parser(vector<Tok> v):t(move(v)){}
    Tok& peek(int o=0){return t[p+o];}
    bool at(int k){return peek().k==k;}
    bool eat(int k){if(at(k)){p++;return true;}return false;}
    Tok need(int k){if(!at(k)){cerr<<"parse error line "<<peek().line<<"\n"; exit(1);} return t[p++];}
    bool isbt(int k){return k==TK_INTTK||k==TK_FLOATTK;}
    Base btype(){if(eat(TK_INTTK))return BI; if(eat(TK_FLOATTK))return BF; need(TK_VOID); return BV;}
    Expr* number(){
        if(at(TK_INT)){auto x=ne(1); x->s=need(TK_INT).s; x->iv=strtoll(x->s.c_str(),0,0); return x;}
        auto x=ne(2); x->s=need(TK_FLOAT).s; x->fv=strtof(x->s.c_str(),0); return x;
    }
    vector<Expr*> dims(bool first_empty=false){
        vector<Expr*> r;
        while(eat('[')){ if(first_empty&&eat(']')){first_empty=false; continue;} r.push_back(expr()); need(']'); first_empty=false; }
        return r;
    }
    Init* init(){
        auto in=new Init();
        if(eat('{')){ if(!eat('}')){do in->ch.push_back(init()); while(eat(',')); need('}');} return in; }
        in->ex=true; in->e=expr(); return in;
    }
    DeclItem item(bool must_init){
        DeclItem it; it.n=need(TK_ID).s; it.dims=dims(); if(eat('='))it.init=init(); else if(must_init){cerr<<"const init expected\n";exit(1);} return it;
    }
    Decl* decl_after(Base b,bool cn,DeclItem first={}){
        auto d=new Decl(); d->b=b; d->cn=cn; if(!first.n.empty())d->a.push_back(first); else d->a.push_back(item(cn));
        while(eat(','))d->a.push_back(item(cn)); need(';'); return d;
    }
    Decl* decl(){
        bool cn=eat(TK_CONST); Base b=btype(); return decl_after(b,cn);
    }
    Param param(){
        Param p; p.b=btype(); p.n=need(TK_ID).s; if(eat('[')){need(']'); p.arr=true; p.dims=dims();} return p;
    }
    Stmt* block(){
        auto s=ns(1); need('{');
        while(!eat('}')){
            if(at(TK_CONST)||isbt(peek().k)){auto x=ns(2); x->d=decl(); s->ss.push_back(x);}
            else s->ss.push_back(stmt());
        }
        return s;
    }
    bool try_lval(Expr*& e){
        if(!at(TK_ID)||peek(1).k=='(')return false;
        e=ne(3); e->s=need(TK_ID).s; while(eat('[')){e->a.push_back(expr()); need(']');}
        return true;
    }
    Stmt* stmt(){
        if(at('{'))return block();
        if(eat(';'))return ns(3);
        if(eat(TK_IF)){auto s=ns(4); need('('); s->e=expr(); need(')'); s->ss.push_back(stmt()); if(eat(TK_ELSE))s->ss.push_back(stmt()); return s;}
        if(eat(TK_WHILE)){auto s=ns(5); need('('); s->e=expr(); need(')'); s->ss.push_back(stmt()); return s;}
        if(eat(TK_BREAK)){need(';'); return ns(6);}
        if(eat(TK_CONTINUE)){need(';'); return ns(7);}
        if(eat(TK_RETURN)){auto s=ns(8); if(!eat(';')){s->e=expr(); need(';');} return s;}
        if(at(TK_ID)&&(peek().s=="printf"||peek().s=="putf")&&peek(1).k=='('&&peek(2).k==TK_STR){
            auto s=ns(10); p+=2; s->s=need(TK_STR).s; while(eat(','))s->es.push_back(expr()); need(')'); need(';'); return s;
        }
        size_t q=p; Expr* lv=nullptr;
        if(try_lval(lv)&&eat('=')){auto s=ns(9); s->e=lv; s->e2=expr(); need(';'); return s;}
        p=q; auto s=ns(11); if(!eat(';')){s->e=expr(); need(';');} return s;
    }
    Expr* primary(){
        if(eat('(')){auto x=expr(); need(')'); return x;}
        if(at(TK_INT)||at(TK_FLOAT))return number();
        Expr* x=nullptr; if(try_lval(x))return x;
        cerr<<"bad primary line "<<peek().line<<"\n"; exit(1);
    }
    Expr* unary(){
        if(at('+')||at('-')||at('!')){auto x=ne(5); x->op=need(peek().k).k; x->a.push_back(unary()); return x;}
        if(at(TK_ID)&&peek(1).k=='('){auto x=ne(4); x->s=need(TK_ID).s; need('('); if(!eat(')')){do x->a.push_back(expr()); while(eat(',')); need(')');} return x;}
        return primary();
    }
    Expr* bin(function<Expr*()> sub, initializer_list<int> ops){
        auto x=sub(); for(;;){int k=peek().k; bool ok=false; for(int o:ops)if(k==o)ok=true; if(!ok)break; auto y=ne(6); y->op=need(k).k; y->a={x,sub()}; x=y;} return x;
    }
    Expr* mul(){return bin([&]{return unary();},{'*','/','%'});}
    Expr* add(){return bin([&]{return mul();},{'+','-'});}
    Expr* rel(){return bin([&]{return add();},{'<','>',TK_LE,TK_GE});}
    Expr* eq(){return bin([&]{return rel();},{TK_EQ,TK_NE});}
    Expr* land(){return bin([&]{return eq();},{TK_AND});}
    Expr* lor(){return bin([&]{return land();},{TK_OR});}
    Expr* expr(){return lor();}
    vector<Top> program(){
        vector<Top> r;
        while(!at(TK_EOF)){
            if(at(TK_CONST)){Top x; x.d=decl(); r.push_back(x); continue;}
            Base b=btype(); string n=need(TK_ID).s;
            if(eat('(')){
                auto f=new Func(); f->r=b; f->n=n;
                if(!eat(')')){do f->ps.push_back(param()); while(eat(',')); need(')');}
                f->body=block(); Top x; x.f=f; r.push_back(x);
            }else{
                DeclItem it; it.n=n; it.dims=dims(); if(eat('='))it.init=init();
                Top x; x.d=decl_after(b,false,it); r.push_back(x);
            }
        }
        return r;
    }
};

struct Type{Base b=BI; bool ptr=false; vector<int> d;};
struct Val{Base b=BI; bool ptr=false; vector<int> d; bool isvoid=false;};
struct CVal{Base b=BI; double f=0; long long i=0;};
struct Sym{Type t; bool glob=false,cn=false,hasc=false; CVal c; string lab; int off=0;};
struct Sig{Base r=BI; vector<Type> ps;};

static int al(int x,int a){return (x+a-1)/a*a;}
static int prod(const vector<int>& v,int l=0){long long r=1; for(int i=l;i<(int)v.size();++i)r*=max(1,v[i]); return (int)r;}
static int fbits(float f){uint32_t u; memcpy(&u,&f,4); return (int)u;}

struct CG{
    vector<Top> tops; unordered_map<string,Sig> sig; vector<unique_ptr<Sym>> all;
    vector<unordered_map<string,Sym*>> sc; ostringstream gdat,gbss,text,*o=nullptr;
    int loc=0,dyn=0,lid=0; string fn,retlab; Base fret=BI; vector<pair<string,string>> loops;
    CG(vector<Top> t):tops(move(t)){builtins();}
    Sym* sym(const string& n){for(int i=(int)sc.size()-1;i>=0;--i)if(sc[i].count(n))return sc[i][n]; cerr<<"unknown "<<n<<"\n"; exit(1);}
    Sym* add(const string& n,Type t,bool glob,bool cn=false){all.emplace_back(new Sym()); auto s=all.back().get(); s->t=t;s->glob=glob;s->cn=cn;s->lab=n;sc.back()[n]=s;return s;}
    void builtins(){
        auto T=[&](Base b,bool p=false,vector<int>d={}){Type t;t.b=b;t.ptr=p;t.d=d;return t;};
        sig["getint"]={BI,{}}; sig["getch"]={BI,{}}; sig["getfloat"]={BF,{}};
        sig["getarray"]={BI,{T(BI,true)}}; sig["getfarray"]={BI,{T(BF,true)}};
        sig["putint"]={BV,{T(BI)}}; sig["putch"]={BV,{T(BI)}}; sig["putfloat"]={BV,{T(BF)}};
        sig["putarray"]={BV,{T(BI),T(BI,true)}}; sig["putfarray"]={BV,{T(BI),T(BF,true)}};
    }
    string L(string p){return ".L_"+fn+"_"+p+to_string(lid++);}
    void e(const string& s){(*o)<<s<<"\n";}
    void adj(const string& r,int v){if(v>=-2048&&v<=2047)e("  addi "+r+", "+r+", "+to_string(v));else{e("  li t6, "+to_string(v));e("  add "+r+", "+r+", t6");}}
    void mem(const string& op,const string& r,int off,const string& b){if(off>=-2048&&off<=2047)e("  "+op+" "+r+", "+to_string(off)+"("+b+")");else{e("  li t6, "+to_string(off));e("  add t6, "+b+", t6");e("  "+op+" "+r+", 0(t6)");}}
    void addr(const string& r,const string& b,int off){if(off>=-2048&&off<=2047)e("  addi "+r+", "+b+", "+to_string(off));else{e("  li t6, "+to_string(off));e("  add "+r+", "+b+", t6");}}
    int alloc(int bytes,int a=8){loc=al(loc+bytes,a); return -loc;}
    void pushi(const string& r="a0"){adj("sp",-8); dyn+=8; mem("sd",r,0,"sp");}
    void pushf(const string& r="fa0"){adj("sp",-8); dyn+=8; mem("fsw",r,0,"sp");}
    void popi(const string& r){mem("ld",r,0,"sp"); adj("sp",8); dyn-=8;}
    void popf(const string& r){mem("flw",r,0,"sp"); adj("sp",8); dyn-=8;}
    CVal cv(Expr* x){
        if(!x)return {};
        if(x->k==1)return {BI,0,x->iv};
        if(x->k==2)return {BF,x->fv,(int)x->fv};
        if(x->k==3){auto s=sym(x->s); if(s->hasc)return s->c; return {BI,0,0};}
        if(x->k==5){auto a=cv(x->a[0]); if(x->op=='-'){if(a.b==BF)a.f=-a.f; else a.i=-a.i;} else if(x->op=='!'){a={BI,0,(a.b==BF? a.f==0:a.i==0)};} return a;}
        if(x->k==6){
            auto l=cv(x->a[0]),r=cv(x->a[1]); bool fl=l.b==BF||r.b==BF; double A=l.b==BF?l.f:l.i,B=r.b==BF?r.f:r.i; long long ai=(long long)A,bi=(long long)B;
            switch(x->op){case '+':return fl?CVal{BF,A+B,(long long)(A+B)}:CVal{BI,0,ai+bi};case '-':return fl?CVal{BF,A-B,(long long)(A-B)}:CVal{BI,0,ai-bi};
            case '*':return fl?CVal{BF,A*B,(long long)(A*B)}:CVal{BI,0,ai*bi};case '/':return fl?CVal{BF,A/B,(long long)(A/B)}:CVal{BI,0,bi?ai/bi:0};
            case '%':return {BI,0,bi?ai%bi:0};case '<':return {BI,0,A<B};case '>':return {BI,0,A>B};case TK_LE:return {BI,0,A<=B};case TK_GE:return {BI,0,A>=B};
            case TK_EQ:return {BI,0,A==B};case TK_NE:return {BI,0,A!=B};case TK_AND:return {BI,0,(A!=0)&&(B!=0)};case TK_OR:return {BI,0,(A!=0)||(B!=0)};}
        }
        return {};
    }
    int cexpr(Expr* x){auto v=cv(x); return v.b==BF?(int)v.f:(int)v.i;}
    Type type(Base b,const vector<Expr*>& ds,bool ptr=false){Type t;t.b=b;t.ptr=ptr; for(auto x:ds)t.d.push_back(cexpr(x)); return t;}
    int sz(Type t){return t.ptr?8:(t.d.empty()?4:4*prod(t.d));}
    vector<Expr*> flat(Init* in,const vector<int>& d){
        vector<Expr*> r(prod(d),nullptr); int pos=0;
        function<void(Init*,int)> rec=[&](Init* z,int lev){
            if(!z)return;
            if(z->ex){if(pos<(int)r.size())r[pos++]=z->e; return;}
            int sub=lev<(int)d.size()?prod(d,lev+1):1;
            for(auto c:z->ch){
                if(c->ex)rec(c,(int)d.size());
                else{int st=pos; rec(c,lev+1); pos=st+sub;}
            }
        };
        rec(in,0); return r;
    }
    void global_decl(Decl* d){
        for(auto &it:d->a){
            Type t=type(d->b,it.dims); Sym* s=add(it.n,t,true,d->cn);
            int n=max(1,prod(t.d)); vector<CVal> vals(n,{t.b,0,0});
            if(it.init){auto fs=t.d.empty()?vector<Expr*>{it.init->ex?it.init->e:nullptr}:flat(it.init,t.d); for(int i=0;i<n&&i<(int)fs.size();++i)if(fs[i])vals[i]=cv(fs[i]);}
            if(d->cn&&t.d.empty()){s->hasc=true; s->c=vals[0];}
            bool zero=true; for(auto v:vals)if((t.b==BF?v.f:v.i)!=0)zero=false;
            auto& out=zero?gbss:gdat; out<<"  .globl "<<it.n<<"\n  .align 2\n"<<it.n<<":\n";
            if(zero)out<<"  .zero "<<sz(t)<<"\n"; else for(auto v:vals)out<<"  .word "<<(t.b==BF?fbits((float)v.f):(int)v.i)<<"\n";
        }
    }
    Type ptype(const Param& p){return type(p.b,p.dims,p.arr);}
    void collect(){
        sc.push_back({});
        for(auto &x:tops)if(x.d)global_decl(x.d);
        for(auto &x:tops)if(x.f){Sig s; s.r=x.f->r; for(auto&p:x.f->ps)s.ps.push_back(ptype(p)); sig[x.f->n]=s;}
    }
    void tof(Val v){if(v.b!=BF){e("  fcvt.s.w fa0, a0");}}
    void toi(Val v){if(v.b==BF)e("  fcvt.w.s a0, fa0, rtz");}
    Val load_lval(Expr* x){
        Val v=addr_lval(x); if(v.ptr||!v.d.empty())return v;
        if(v.b==BF)mem("flw","fa0",0,"a0"); else mem("lw","a0",0,"a0"); return v;
    }
    Val addr_lval(Expr* x){
        Sym* s=sym(x->s); Type t=s->t;
        if(s->glob)e("  la a0, "+s->lab); else if(t.ptr)mem("ld","a0",s->off,"s0"); else addr("a0","s0",s->off);
        int m=x->a.size();
        for(int i=0;i<m;i++){
            pushi(); Val idx=gen(x->a[i]); toi(idx); popi("t0");
            int stride=t.ptr?(i==0?prod(t.d):prod(t.d,i)):prod(t.d,i+1);
            e("  li t1, "+to_string(stride*4)); e("  mul a0, a0, t1"); e("  add a0, t0, a0");
        }
        Val v; v.b=t.b; v.ptr=false;
        int rank=t.ptr?(1+(int)t.d.size()):(int)t.d.size();
        if(m<rank){v.ptr=true; if(t.ptr){for(int i=max(0,m-1);i<(int)t.d.size();++i)v.d.push_back(t.d[i]);}else for(int i=m;i<(int)t.d.size();++i)v.d.push_back(t.d[i]);}
        return v;
    }
    void cond(Expr* x,const string& T,const string& F){
        if(x->k==6&&x->op==TK_AND){string m=L("and"); cond(x->a[0],m,F); e(m+":"); cond(x->a[1],T,F); return;}
        if(x->k==6&&x->op==TK_OR){string m=L("or"); cond(x->a[0],T,m); e(m+":"); cond(x->a[1],T,F); return;}
        Val v=gen(x); if(v.b==BF){e("  fmv.w.x ft0, zero"); e("  feq.s t0, fa0, ft0"); e("  beqz t0, "+T);} else e("  bnez a0, "+T); e("  j "+F);
    }
    Val binary(Expr* x){
        if(x->op==TK_AND||x->op==TK_OR){string T=L("t"),F=L("f"),E=L("e"); cond(x,T,F); e(T+":"); e("  li a0, 1"); e("  j "+E); e(F+":"); e("  li a0, 0"); e(E+":"); return {BI};}
        Val l=gen(x->a[0]); if(l.b==BF&&!l.ptr)pushf(); else pushi(); Val r=gen(x->a[1]);
        bool fl=(l.b==BF||r.b==BF)&&x->op!='%';
        if(fl){
            if(r.b!=BF)tof(r);
            if(l.b==BF)popf("ft0"); else{popi("t0"); e("  fcvt.s.w ft0, t0");}
            switch(x->op){case '+':e("  fadd.s fa0, ft0, fa0");return {BF};case '-':e("  fsub.s fa0, ft0, fa0");return {BF};case '*':e("  fmul.s fa0, ft0, fa0");return {BF};case '/':e("  fdiv.s fa0, ft0, fa0");return {BF};
            case '<':e("  flt.s a0, ft0, fa0");return {BI};case '>':e("  flt.s a0, fa0, ft0");return {BI};case TK_LE:e("  fle.s a0, ft0, fa0");return {BI};case TK_GE:e("  fle.s a0, fa0, ft0");return {BI};
            case TK_EQ:e("  feq.s a0, ft0, fa0");return {BI};case TK_NE:e("  feq.s a0, ft0, fa0");e("  seqz a0, a0");return {BI};}
        }else{
            toi(r); popi("t0");
            switch(x->op){case '+':e("  addw a0, t0, a0");break;case '-':e("  subw a0, t0, a0");break;case '*':e("  mulw a0, t0, a0");break;case '/':e("  divw a0, t0, a0");break;case '%':e("  remw a0, t0, a0");break;
            case '<':e("  slt a0, t0, a0");break;case '>':e("  slt a0, a0, t0");break;case TK_LE:e("  slt a0, a0, t0");e("  xori a0, a0, 1");break;case TK_GE:e("  slt a0, t0, a0");e("  xori a0, a0, 1");break;
            case TK_EQ:e("  xor a0, t0, a0");e("  seqz a0, a0");break;case TK_NE:e("  xor a0, t0, a0");e("  snez a0, a0");break;}
            return {BI};
        }
        return {BI};
    }
    Val call(Expr* x){
        string name=x->s;
        if(name=="starttime"||name=="stoptime"){e("  li a0, 0"); e("  call "+string(name=="starttime"?"_sysy_starttime":"_sysy_stoptime")); return {BV,false,{},true};}
        Sig sg=sig.count(name)?sig[name]:Sig{BI,{}};
        int n=x->a.size(), ir=0,fr=0,stk=0; vector<Base> bs(n); vector<bool> isptr(n);
        for(int i=0;i<n;i++){
            Type et=i<(int)sg.ps.size()?sg.ps[i]:Type{BI};
            Val v=gen(x->a[i]); if(!et.ptr){if(et.b==BF)tof(v); else toi(v);}
            bs[i]=et.ptr?BI:et.b; isptr[i]=et.ptr;
            if(bs[i]==BF)pushf(); else pushi();
        }
        for(int i=0;i<n;i++){Base b=bs[i]; if(b==BF){if(fr<8)fr++; else stk++;}else{if(ir<8)ir++; else stk++;}}
        int out=stk*8, pad=(16-((dyn+out)%16))%16; if(out+pad){adj("sp",-(out+pad)); dyn+=out+pad;}
        ir=fr=stk=0;
        for(int i=0;i<n;i++){
            int off=out+pad+8*(n-1-i); Base b=bs[i];
            if(b==BF){ if(fr<8){mem("flw","fa"+to_string(fr),off,"sp"); fr++;} else {mem("flw","ft0",off,"sp"); mem("fsw","ft0",stk*8,"sp"); stk++;}}
            else { if(ir<8){mem("ld","a"+to_string(ir),off,"sp"); ir++;} else {mem("ld","t0",off,"sp"); mem("sd","t0",stk*8,"sp"); stk++;}}
        }
        e("  call "+name);
        if(out+pad+n*8){adj("sp",out+pad+n*8); dyn-=out+pad+n*8;}
        return {sg.r,false,{},sg.r==BV};
    }
    Val gen(Expr* x){
        if(!x)return {BI};
        if(x->k==1){e("  li a0, "+to_string((int)x->iv)); return {BI};}
        if(x->k==2){e("  li t0, "+to_string(fbits(x->fv))); e("  fmv.w.x fa0, t0"); return {BF};}
        if(x->k==3){Sym* s=sym(x->s); if(s->hasc&&x->a.empty()){if(s->c.b==BF){e("  li t0, "+to_string(fbits((float)s->c.f)));e("  fmv.w.x fa0, t0");return {BF};} e("  li a0, "+to_string((int)s->c.i));return {BI};} return load_lval(x);}
        if(x->k==4)return call(x);
        if(x->k==5){Val v=gen(x->a[0]); if(x->op=='-'){if(v.b==BF)e("  fneg.s fa0, fa0"); else e("  negw a0, a0");} else if(x->op=='!'){string T=L("notT"),F=L("notF"),E=L("notE"); if(v.b==BF){e("  fmv.w.x ft0, zero");e("  feq.s a0, fa0, ft0");}else e("  seqz a0, a0"); v.b=BI;} return v;}
        if(x->k==6)return binary(x);
        return {BI};
    }
    void store_to_addr(Base b,Val v){ if(b==BF){tof(v); mem("fsw","fa0",0,"t0");} else {toi(v); mem("sw","a0",0,"t0");}}
    void local_decl(Decl* d){
        for(auto &it:d->a){
            Type t=type(d->b,it.dims); Sym* s=add(it.n,t,false,d->cn); s->off=alloc(sz(t), t.ptr?8:4);
            if(d->cn&&t.d.empty()&&it.init&&it.init->ex){s->hasc=true; s->c=cv(it.init->e);}
            if(t.d.empty()){ if(it.init){Val v=gen(it.init->ex?it.init->e:nullptr); if(t.b==BF){tof(v); mem("fsw","fa0",s->off,"s0");}else{toi(v); mem("sw","a0",s->off,"s0");}} continue; }
            int n=prod(t.d); for(int i=0;i<n;i++){addr("t0","s0",s->off+i*4); e("  sw zero, 0(t0)");}
            if(it.init){auto fs=flat(it.init,t.d); for(int i=0;i<n&&i<(int)fs.size();++i)if(fs[i]){Val v=gen(fs[i]); addr("t0","s0",s->off+i*4); store_to_addr(t.b,v);}}
        }
    }
    void print(Stmt* s){
        int ai=0; auto putch=[&](int c){e("  li a0, "+to_string(c)); e("  call putch");};
        for(size_t i=0;i<s->s.size();++i){
            if(s->s[i]=='\\'&&i+1<s->s.size()){char q=s->s[++i]; putch(q=='n'?10:q=='t'?9:q); continue;}
            if(s->s[i]=='%'&&i+1<s->s.size()){
                char q=s->s[++i]; if(q=='%'){putch('%'); continue;} if(ai>=(int)s->es.size())continue;
                Val v=gen(s->es[ai++]); if(q=='f'){tof(v); e("  call putfloat");} else {toi(v); e(string("  call ")+(q=='c'?"putch":"putint"));} continue;
            }
            putch((unsigned char)s->s[i]);
        }
    }
    void stmt(Stmt* s){
        if(!s)return;
        if(s->k==1){sc.push_back({}); for(auto x:s->ss)stmt(x); sc.pop_back();}
        else if(s->k==2)local_decl(s->d);
        else if(s->k==4){string T=L("then"),F=L("else"),E=L("endif"); cond(s->e,T,s->ss.size()>1?F:E); e(T+":"); stmt(s->ss[0]); e("  j "+E); if(s->ss.size()>1){e(F+":"); stmt(s->ss[1]);} e(E+":");}
        else if(s->k==5){string C=L("while"),B=L("body"),E=L("wend"); loops.push_back({E,C}); e(C+":"); cond(s->e,B,E); e(B+":"); stmt(s->ss[0]); e("  j "+C); e(E+":"); loops.pop_back();}
        else if(s->k==6){e("  j "+loops.back().first);}
        else if(s->k==7){e("  j "+loops.back().second);}
        else if(s->k==8){if(s->e){Val v=gen(s->e); if(fret==BF)tof(v); else if(fret==BI)toi(v);} e("  j "+retlab);}
        else if(s->k==9){Val a=addr_lval(s->e); pushi(); Val v=gen(s->e2); popi("t0"); store_to_addr(a.b,v);}
        else if(s->k==10)print(s);
        else if(s->k==11&&s->e)gen(s->e);
    }
    void func(Func* f){
        fn=f->n; fret=f->r; loc=0; dyn=0; lid=0; retlab=".L_"+fn+"_ret"; o=new ostringstream; sc.push_back({});
        int ir=0,fr=0,stk=0;
        for(auto &p:f->ps){
            Type t=ptype(p); Sym* s=add(p.n,t,false); s->off=alloc(t.ptr?8:4, t.ptr?8:4);
            if(t.ptr||t.b==BI){ if(ir<8)mem("sd","a"+to_string(ir++),s->off,"s0"); else{mem("ld","t0",stk++*8,"s0"); mem("sd","t0",s->off,"s0");}}
            else { if(fr<8)mem("fsw","fa"+to_string(fr++),s->off,"s0"); else{mem("flw","ft0",stk++*8,"s0"); mem("fsw","ft0",s->off,"s0");}}
        }
        stmt(f->body);
        e(retlab+":");
        string body=o->str(); delete o; o=nullptr; sc.pop_back();
        int frame=al(loc+16,16);
        text<<"  .text\n  .globl "<<f->n<<"\n  .align 2\n"<<f->n<<":\n";
        auto old=o; o=&text; adj("sp",-frame); mem("sd","ra",frame-8,"sp"); mem("sd","s0",frame-16,"sp"); addr("s0","sp",frame); o=old;
        text<<body;
        o=&text; addr("sp","s0",-frame); mem("ld","ra",frame-8,"sp"); mem("ld","s0",frame-16,"sp"); adj("sp",frame); e("  ret"); o=nullptr;
    }
    string run(){
        collect(); for(auto &x:tops)if(x.f)func(x.f);
        ostringstream out; out<<"  .attribute arch, \"rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0\"\n  .option nopic\n  .option norelax\n";
        if(gdat.tellp()>0)out<<"  .data\n"<<gdat.str(); if(gbss.tellp()>0)out<<"  .bss\n"<<gbss.str(); out<<text.str(); return out.str();
    }
};

int main(int argc,char**argv){
    string in,out="output.s";
    for(int i=1;i<argc;i++){string a=argv[i]; if(a=="-o"&&i+1<argc)out=argv[++i]; else if(!a.empty()&&a[0]!='-')in=a;}
    if(in.empty())return 1;
    ifstream f(in); string src((istreambuf_iterator<char>(f)),{}); if(!f)return 1;
    Lexer lx(src); lx.run(); Parser ps(lx.ts); auto prog=ps.program(); CG cg(prog); ofstream fo(out); fo<<cg.run(); return 0;
}
