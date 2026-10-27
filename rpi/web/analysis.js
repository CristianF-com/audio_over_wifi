/*ANALYSIS_BEGIN*/
function makeChirp(sr,f0,f1,chirpMs,fadeMs){
  const n=Math.floor(chirpMs/1000*sr),T=chirpMs/1000,r=Math.log(f1/f0),K=2*Math.PI*f0*T/r,out=new Float32Array(n);
  for(let i=0;i<n;i++){const t=i/sr,tu=t*1e6,fu=fadeMs*1000;let w=1;
    if(tu<fu)w=0.5-0.5*Math.cos(Math.PI*tu/fu);else if(tu>chirpMs*1000-fu)w=0.5-0.5*Math.cos(Math.PI*(chirpMs*1000-tu)/fu);
    out[i]=Math.sin(K*(Math.exp(t*r/T)-1))*w;}
  return out;
}
function fft(re,im,inv){
  const n=re.length;
  for(let i=1,j=0;i<n;i++){let bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;
    if(i<j){let t=re[i];re[i]=re[j];re[j]=t;t=im[i];im[i]=im[j];im[j]=t;}}
  for(let len=2;len<=n;len<<=1){const ang=2*Math.PI/len*(inv?1:-1),wr=Math.cos(ang),wi=Math.sin(ang),h=len>>1;
    for(let i=0;i<n;i+=len){let cr=1,ci=0;
      for(let j=0;j<h;j++){const a=i+j,b=a+h,vr=re[b]*cr-im[b]*ci,vi=re[b]*ci+im[b]*cr;
        re[b]=re[a]-vr;im[b]=im[a]-vi;re[a]+=vr;im[a]+=vi;const nc=cr*wr-ci*wi;ci=cr*wi+ci*wr;cr=nc;}}}
  if(inv)for(let i=0;i<n;i++){re[i]/=n;im[i]/=n;}
}
// Correlación cruzada rec ⋆ tmpl por bloques (overlap-save): c[n]=Σ rec[n+m]·tmpl[m]
function xcorr(rec,tmpl,N){
  const M=tmpl.length,step=N-M+1,out=new Float32Array(rec.length),tr=new Float64Array(N),ti=new Float64Array(N);
  tr.set(tmpl);fft(tr,ti,false);
  const sr=new Float64Array(N),si=new Float64Array(N);
  for(let s=0;s<rec.length;s+=step){
    sr.fill(0);si.fill(0);const e=Math.min(rec.length,s+N);
    for(let i=s;i<e;i++)sr[i-s]=rec[i];
    fft(sr,si,false);
    for(let i=0;i<N;i++){const a=sr[i],b=si[i];sr[i]=a*tr[i]+b*ti[i];si[i]=b*tr[i]-a*ti[i];}  // R·conj(T)
    fft(sr,si,true);
    const lim=Math.min(step,rec.length-s);
    for(let i=0;i<lim;i++)out[s+i]=sr[i];
  }
  return out;
}
function power(rec,a,b){a=Math.max(0,Math.floor(a));b=Math.min(rec.length,Math.floor(b));let s=0;
  for(let i=a;i<b;i++)s+=rec[i]*rec[i];return b>a?s/(b-a):0;}
// Potencia por banda (lineal) de rec[start .. start+lenS) mediante FFT. El chirp logarítmico reparte la misma energía en cada octava,
// así que las potencias de las bandas son comparables entre sí y entre altavoces.
function bandPow(rec,sr,start,lenS,edges){
  let N=1;while(N<lenS)N<<=1;const re=new Float64Array(N),im=new Float64Array(N),a=Math.floor(start);
  for(let i=0;i<lenS;i++){const j=a+i;if(j>=0&&j<rec.length)re[i]=rec[j];}
  fft(re,im,false);
  const out=new Float64Array(edges.length-1),df=sr/N;
  for(let b=0;b<out.length;b++){const k0=Math.ceil(edges[b]/df),k1=Math.min(N/2-1,Math.floor(edges[b+1]/df));let t=0;
    for(let k=k0;k<=k1;k++)t+=re[k]*re[k]+im[k]*im[k];out[b]=t;}
  return out;
}
// p = {slot_ms,chirp_ms,f0,f1,fade_ms,sequence:[ids], expect_s?, eq_edges?:[Hz…]}.  expect_s = instante (s, en la grabación) en que
// debería sonar el 1er chirp; evita que un nodo mudo desplace la secuencia un slot (ambigüedad de periodicidad).
function analyze(rec,sr,p){
  const tm=makeChirp(sr,p.f0,p.f1,p.chirp_ms,p.fade_ms),c=xcorr(rec,tm,65536);
  const B=Math.max(1,Math.round(sr/1000)),nb=Math.floor(c.length/B),bm=new Float32Array(nb);
  for(let j=0;j<nb;j++){let m=0;for(let i=j*B;i<(j+1)*B;i++){const a=Math.abs(c[i]);if(a>m)m=a;}bm[j]=m;}
  const med=Math.max(Float32Array.from(bm).sort()[nb>>1],1e-12),W=60,mm=new Float32Array(nb);
  for(let j=0;j<nb;j++){let m=0;for(let i=Math.max(0,j-W);i<=Math.min(nb-1,j+W);i++)if(bm[i]>m)m=bm[i];mm[j]=m;}
  const L=p.sequence.length,slot=p.slot_ms,slotS=slot/1000*sr;
  if(nb<(L-1)*slot+1)return{error:'Grabación demasiado corta'};
  let best=-1e300,bj=0,jlo=0,jhi=nb;
  if(p.expect_s!=null){jlo=Math.max(0,Math.floor((p.expect_s-0.65)*1000));jhi=Math.ceil((p.expect_s+0.65)*1000)+1;}
  for(let j=jlo;j<jhi&&j+(L-1)*slot<nb;j++){let sc=0;for(let k=0;k<L;k++)sc+=Math.log(Math.max(mm[j+k*slot]/med,1));if(sc>best){best=sc;bj=j;}}
  const peakIn=(lo,hi)=>{lo=Math.max(1,Math.floor(lo));hi=Math.min(c.length-2,Math.floor(hi));let bi=lo,bv=0;
    for(let i=lo;i<=hi;i++){const a=Math.abs(c[i]);if(a>bv){bv=a;bi=i;}}
    const y0=Math.abs(c[bi-1]),y1=Math.abs(c[bi]),y2=Math.abs(c[bi+1]),d=y0-2*y1+y2;
    return{pos:bi+(Math.abs(d)>1e-20?0.5*(y0-y2)/d:0),val:bv};};
  const p0=peakIn((bj-W-5)*B,(bj+W+5)*B).pos,slots=[];
  for(let k=0;k<L;k++){const ex=p0+k*slotS,pk=peakIn(ex-W*B,ex+W*B);
    slots.push({k,id:p.sequence[k],pos:pk.pos,snr:pk.val/med});}
  const nEnd=Math.min(1.0*sr,p0-0.3*sr),Pn=power(rec,0.1*sr,nEnd);
  const edges=p.eq_edges||null,segLen=Math.round((p.chirp_ms+34)/1000*sr);
  let Nb=null;                                   // ruido por banda (misma longitud de ventana que el chirp)
  if(edges&&0.1*sr+segLen<p0-0.1*sr)Nb=bandPow(rec,sr,0.1*sr,segLen,edges);
  const per={};
  for(const s of slots){
    const d=((s.pos-slots[0].pos)/sr-s.k*slot/1000)*1e6;
    const P=power(rec,s.pos,s.pos+p.chirp_ms/1000*sr);
    const o=per[s.id]||(per[s.id]={id:s.id,d:[],P:[],snr:1e300,B:[]});
    o.d.push(d);o.P.push(P);o.snr=Math.min(o.snr,s.snr);
    if(edges)o.B.push(bandPow(rec,sr,s.pos-0.004*sr,segLen,edges));}
  const nodes=Object.values(per).sort((a,b)=>a.id-b.id).map(o=>{
    const dm=o.d.reduce((a,b)=>a+b,0)/o.d.length,Pm=o.P.reduce((a,b)=>a+b,0)/o.P.length;
    const r={id:o.id,d_us:dm,spread_us:Math.max(...o.d)-Math.min(...o.d),
      level_db:10*Math.log10(Math.max(Pm-Pn,1e-14)),snr:o.snr,ok:o.snr>=8&&Pm>Pn*2,bands:null,band_ok:null};
    if(edges&&o.B.length){const nb=edges.length-1;r.bands=[];r.band_ok=[];
      for(let b=0;b<nb;b++){let t=0;for(const B of o.B)t+=B[b];t/=o.B.length;const nz=Nb?Nb[b]:0;
        r.band_ok.push(t>nz*4&&t>1e-12);r.bands.push(10*Math.log10(Math.max(t-nz,1e-14)));}}
    return r;});
  return{nodes,slots,noise_db:10*Math.log10(Math.max(Pn,1e-14))};
}
// EQ propuesto: iguala la respuesta por bandas de cada altavoz a la MEDIA de los altavoces medidos (el mismo micrófono mide
// a todos, así su propia respuesta se cancela). Se quita el nivel global (eso lo corrige el volumen) y se limita a ±6 dB.
function propose(nodes,volIncl,eqIncl){
  volIncl=volIncl||{};eqIncl=eqIncl||{};
  const good=nodes.filter(n=>n.ok);if(!good.length)return nodes;
  const dmax=Math.max(...good.map(n=>n.d_us));
  const lv=good.filter(n=>volIncl[n.id]!==false).map(n=>n.level_db),Lmin=lv.length?Math.min(...lv):0;
  const eqn=good.filter(n=>n.bands&&eqIncl[n.id]!==false),nb=eqn.length?eqn[0].bands.length:0,R=[];
  for(let b=0;b<nb;b++){const v=eqn.filter(n=>n.band_ok[b]).map(n=>n.bands[b]);R.push(v.length>=2?v.reduce((x,y)=>x+y,0)/v.length:null);}
  return nodes.map(n=>{const o=Object.assign({},n);
    if(n.ok){o.latency_us=Math.max(0,Math.min(500000,Math.round(dmax-n.d_us)));
      o.volume=(volIncl[n.id]!==false)?Math.max(1,Math.min(255,Math.round(255*Math.pow(10,(Lmin-n.level_db)/20)))):255;}
    o.eq=null;
    if(n.ok&&n.bands&&eqIncl[n.id]!==false&&eqn.length>=2){
      const d=n.bands.map((x,b)=>(R[b]!=null&&n.band_ok[b])?R[b]-x:null),dv=d.filter(x=>x!=null);
      if(dv.length){const m=dv.reduce((x,y)=>x+y,0)/dv.length;
        o.eq=d.map(x=>x==null?0:Math.round(Math.max(-6,Math.min(6,x-m))*10)/10);}}
    return o;});
}
/*ANALYSIS_END*/
