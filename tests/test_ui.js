// Pruebas de interfaz con Playwright (Chromium): panel, /ble (Web Bluetooth simulado) y /calibrate (resultados inyectados).
// Requiere el arnés en :18080 y feed_nodes.py N. Uso: NODE_PATH=$(npm root -g) node test_ui.js [carpeta_capturas]
const {chromium}=require('playwright');
const BASE='http://127.0.0.1:18080', SHOTS=process.argv[2]||null;
let fails=0;const ok=(n,c,x)=>{console.log((c?'PASS ':'FAIL ')+'UI '+n+(c?'':'  '+(x===undefined?'':JSON.stringify(x))));if(!c)fails++;};
const J=async(p)=>(await fetch(BASE+p)).json();
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function until(f,ms=4000){const t=Date.now();while(Date.now()-t<ms){try{if(await f())return true;}catch(e){}await sleep(100);}return false;}
(async()=>{
  const b=await chromium.launch();
  const ctx=await b.newContext({viewport:{width:1280,height:860}});
  const p=await ctx.newPage();const errs=[];p.on('pageerror',e=>errs.push(String(e)));p.on('console',m=>{if(m.type()==='error'&&!m.text().includes('status of 400'))errs.push(m.text());});
  await J('/api/dsp').catch(()=>0);
  await fetch(BASE+'/api/dsp',{method:'POST',body:JSON.stringify({xover:false,eq:false})});
  await fetch(BASE+'/api/layout',{method:'POST',body:JSON.stringify({layout:'stereo5'})});
  await p.goto(BASE+'/');await p.waitForSelector('#spks .spk');await sleep(1200);
  const n=await p.locator('#spks .spk').count();ok('plano: un altavoz por nodo habilitado (5)',n===5,n);
  ok('plano: nodos en línea (punto verde)',await p.locator('#spks .st.on').count()===5);
  ok('estado: «5 de 5 en línea»',(await p.textContent('#onl')).includes('5 de 5'));
  // Estado de transmisión y vúmetros (datos reales del arnés: ch0/1 −6 dBFS, ch2 y ch4 −12 dBFS, el resto en silencio)
  ok('estado: «Transmitiendo · 5 altavoces»',await until(async()=>(await p.textContent('#hs')).includes('Transmitiendo · 5 altavoces')),await p.textContent('#hs'));
  ok('estado: punto verde',(await p.getAttribute('#dot','class')).includes('ok'));
  const lit=await p.evaluate(()=>[...document.querySelectorAll('#mb .bar')].map(b=>b.querySelectorAll('.led.on').length));
  ok('vúmetros LED: 10 canales x 20 segmentos',await p.evaluate(()=>[...document.querySelectorAll('#mb .bar')].every(b=>b.children.length===20))&&lit.length===10);
  ok('vúmetros LED: FL/FR ≈ 18 LED (−6 dBFS), C y canal 4 ≈ 16 (−12 dBFS), LFE apagado',Math.abs(lit[0]-18)<=1&&Math.abs(lit[1]-18)<=1&&Math.abs(lit[2]-16)<=1&&lit[4]>=15&&lit[4]<=17&&lit[3]===0,lit);
  const cs=await p.evaluate(()=>{const b=document.querySelector('#mb .bar'),sg=b.children[0],bs=getComputedStyle(b),ss=getComputedStyle(sg);
    return {cls:[...b.children].map(x=>x.className.split(' ')[1]).join(''),rb:bs.borderRadius,rs:ss.borderRadius,bg:ss.backgroundImage,cl:getComputedStyle(document.getElementById('mcl0')).borderRadius,
      col:[...b.children].filter((x,i)=>[0,14,18].includes(i)).map(x=>getComputedStyle(x).backgroundColor)};});
  ok('vúmetros LED: 14 verdes (hasta −18 dB), 4 amarillos (hasta −6 dB), 2 rojos',cs.cls==='g'.repeat(14)+'y'.repeat(4)+'r'.repeat(2),cs.cls);
  ok('vúmetros LED: colores estándar verde/amarillo/rojo, planos (sin degradado) y sin esquinas redondeadas',cs.col.join('|')==='rgb(48, 209, 88)|rgb(255, 214, 10)|rgb(255, 69, 58)'&&cs.bg==='none'&&cs.rb==='0px'&&cs.rs==='0px'&&cs.cl==='0px',cs);
  ok('vúmetros: texto de pico ≈ −6 dBFS',(await p.textContent('#mtxt')).includes('−')===false&&(await p.textContent('#mtxt')).includes('-6'),await p.textContent('#mtxt'));
  const fake=async(o)=>{await p.route('**/api/levels',r=>r.fulfill({contentType:'application/json',body:JSON.stringify(Object.assign({capturing:true,signal:true,signal_age_ms:10,pps:900,sending:true,nodes_online:5,db:[-6,-6,-12,-90,-12,-90,-90,-90,-90,-90]},o))}));};
  await fake({capturing:false,signal:false,sending:false,db:[-90,-90,-90,-90,-90,-90,-90,-90,-90,-90]});
  ok('estado: sin captura -> «Sin captura de la Traktor» (rojo) y barras atenuadas',await until(async()=>(await p.textContent('#hs'))==='Sin captura de la Traktor'&&(await p.getAttribute('#dot','class')).includes('bad')&&(await p.getAttribute('#meters','class')).includes('off')&&(await p.locator('#mb .led.on').count())===0));
  await fake({signal:false,sending:false,db:[-90,-90,-90,-90,-90,-90,-90,-90,-90,-90]});
  ok('estado: silencio -> «Traktor conectada, sin señal» (ámbar)',await until(async()=>(await p.textContent('#hs'))==='Traktor conectada, sin señal'&&(await p.getAttribute('#dot','class')).includes('warn')&&(await p.textContent('#mtxt'))==='Silencio'));
  await fake({sending:false,nodes_online:0});
  ok('estado: señal sin altavoces -> «Hay señal, ningún altavoz en línea»',await until(async()=>(await p.textContent('#hs'))==='Hay señal, ningún altavoz en línea'));
  await fake({db:[-0.5,-6,-12,-90,-12,-90,-90,-90,-90,-90]});
  ok('vúmetros: clip (≥ −1 dBFS) enciende el indicador del canal y los 2 LED rojos',await until(async()=>(await p.getAttribute('#mcl0','class')).includes('on')&&(await p.locator('#mb .bar').first().locator('.led.r.on').count())===2));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_vumetros_clip.png'});
  await p.unroute('**/api/levels');
  ok('estado: vuelve a «Transmitiendo» al restablecerse',await until(async()=>(await p.textContent('#hs')).startsWith('Transmitiendo')));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_altavoces.png'});
  // Cajón con autoguardado
  await p.click('#spks .spk[data-id="1"]');await p.waitForSelector('#drawer.on');
  ok('cajón abierto con título «Altavoz #1»',(await p.textContent('#dtitle'))==='Altavoz #1');
  await p.fill('#d_vl','128');await p.dispatchEvent('#d_vl','input');
  await p.click('#d_md button[data-v="1"]');
  await p.click('#d_rl button[data-v="2"]');
  await p.fill('#d_eq2','4');await p.dispatchEvent('#d_eq2','input');
  ok('autoguardado: volumen, mono, rol sub y EQ llegan al servidor',await until(async()=>{const x=(await J('/api/nodes'))[1];return x.volume_l===128&&x.mono===true&&x.role===2&&x.volume_r===128&&x.ch_right===x.ch_left&&Math.abs(x.eq[2]-4)<0.01;}),(await J('/api/nodes'))[1]);
  ok('mono oculta el canal derecho',!(await p.isVisible('#d_crw')));
  ok('«Guardado» visible tras autoguardar',await until(async()=>(await p.textContent('#d_ss'))==='Guardado'));
  ok('cambiar mono/rol marca la distribución como personalizada (mono sí es estructural)',(await J('/api/status')).layout===2);
  await p.click('#d_eqon');
  ok('cajón: «Activar» enciende la EQ',await until(async()=>(await J('/api/status')).dsp_eq===true));
  await p.waitForSelector('#d_eqoff');
  await p.click('#d_eqoff');
  ok('cajón: «Desactivar EQ» vuelve a pass-through',await until(async()=>{const s=await J('/api/status');return s.dsp_eq===false&&s.dsp_xover===false;}));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_cajon.png'});
  await p.keyboard.press('Escape');ok('Esc cierra el cajón',await until(async()=>!(await p.$('#drawer.on'))));
  await fetch(BASE+'/api/layout',{method:'POST',body:JSON.stringify({layout:'stereo5'})});
  // Sonido: pass-through ↔ procesado
  await p.click('.tab[data-tab="snd"]');
  ok('por defecto: pass-through activo',(await p.getAttribute('#t_pass','aria-pressed'))==='true'&&(await p.getAttribute('#t_proc','aria-pressed'))==='false');
  ok('por defecto: interruptores apagados',(await p.getAttribute('#sw_xo','aria-checked'))==='false'&&(await p.getAttribute('#sw_eq','aria-checked'))==='false');
  await p.click('#t_proc');
  ok('«Procesado» activa crossover y EQ en el servidor',await until(async()=>{const s=await J('/api/status');return s.dsp_xover===true&&s.dsp_eq===true;}));
  ok('la UI refleja Procesado',await until(async()=>(await p.getAttribute('#t_proc','aria-pressed'))==='true'&&(await p.getAttribute('#sw_eq','aria-checked'))==='true'));
  await p.click('#sw_xo');
  ok('apagar solo el crossover deja la EQ activa',await until(async()=>{const s=await J('/api/status');return s.dsp_xover===false&&s.dsp_eq===true;}));
  ok('con solo EQ sigue en modo Procesado',(await p.getAttribute('#t_pass','aria-pressed'))==='false');
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_sonido_procesado.png'});
  // Volver a pass-through desde fuera de la pestaña Sonido (aviso en «Tu sala» y enlace del cajón del altavoz)
  await p.click('[data-tab=spk]');
  ok('con procesado activo, «Tu sala» muestra el aviso para volver a pass-through',await until(async()=>await p.isVisible('#procbar')));
  await p.click('#procoff');
  ok('el aviso «Volver a pass-through» apaga el procesado',await until(async()=>{const s=await J('/api/status');return s.dsp_xover===false&&s.dsp_eq===false;}));
  ok('y el aviso desaparece',await until(async()=>!(await p.isVisible('#procbar'))));
  await fetch(BASE+'/api/dsp',{method:'POST',body:JSON.stringify({eq:true})});
  await p.click('[data-tab=snd]');
  await p.click('#t_pass');
  ok('«Pass-through» apaga ambos',await until(async()=>{const s=await J('/api/status');return s.dsp_xover===false&&s.dsp_eq===false;}));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_sonido_passthrough.png'});
  // Subwoofer desde la pestaña Sonido
  await p.selectOption('#subsel','4');
  ok('elegir subwoofer asigna roles (4=sub, resto satélite)',await until(async()=>{const a=await J('/api/nodes');return a[4].role===2&&a[0].role===1;}));
  await p.selectOption('#subsel','-');
  ok('«Ninguno» devuelve todos a rango completo',await until(async()=>(await J('/api/nodes')).every(x=>x.role===0)));
  // Latencia
  await p.click('.tab[data-tab="lat"]');await p.click('#plseg button[data-pl="120"]');
  ok('colchón de red 120 ms',await until(async()=>(await J('/api/status')).playout_ms===120));
  ok('tabla de red con 5 filas',await until(async()=>(await p.locator('#health tr').count())===5));
  ok('comando mpv visible',(await p.textContent('#mpvcmd')).includes('--audio-delay='));
  await p.click('#plseg button[data-pl="80"]');
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_latencia.png',fullPage:true});
  // Añadir altavoces por Bluetooth desde la Raspberry (ayudante simulado)
  const fs=require('fs');await p.click('.tab[data-tab="spk"]');
  await p.click('#lble');await p.waitForSelector('#bdrawer.on');await p.waitForSelector('#b_ssid');
  await p.fill('#b_ssid','Casa5G');await p.fill('#b_pass','corta');await p.click('#b_save');
  ok('BLE: contraseña corta -> error visible',await until(async()=>(await p.textContent('#b_ss')).includes('8 y 63')));
  await p.fill('#b_pass','clave12345');await p.click('#b_save');
  ok('BLE: «Guardada» tras guardar la WiFi',await until(async()=>(await p.textContent('#b_ss')).includes('Guardada')));
  await p.click('#b_scan');
  ok('BLE: lista 5 dispositivos',await until(async()=>(await p.locator('#bbody .found').count())===5,8000),await p.locator('#bbody .found').count());
  ok('BLE: 3 nuevos con número sugerido libre (5, 6, 7; el 2 es del que ya está en la red)',await p.evaluate(()=>[...document.querySelectorAll('.found')].filter(f=>f.textContent.includes('Nuevo')).map(f=>f.querySelector('select').value).join())==='5,6,7');
  ok('BLE: números en uso marcados',(await p.locator('.found select').first().innerHTML()).includes('0 (en uso)'));
  ok('BLE: el que no se pudo leer muestra el error',(await p.textContent('#bbody')).includes('No se pudo conectar: timeout'));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_bluetooth.png'});
  fs.writeFileSync('/tmp/sonos_ble_mock.log','');
  await p.click('.found[data-i="0"] [data-id]');
  ok('BLE: «Sonar» llega al ayudante',await until(async()=>fs.readFileSync('/tmp/sonos_ble_mock.log','utf8').includes('"identify"')));
  await p.click('.found[data-i="0"] [data-pv]');
  ok('BLE: Configurar envía WiFi e ID al dispositivo',await until(async()=>{const l=fs.readFileSync('/tmp/sonos_ble_mock.log','utf8');return l.includes('"cmd": "provision"')||l.includes('"cmd":"provision"');}));
  ok('BLE: la tarjeta muestra «Enviado»',await until(async()=>(await p.textContent('.found[data-i="0"] .res')).includes('Enviado')));
  p.once('dialog',d=>d.accept());
  await p.click('#b_all');
  ok('BLE: «Configurar los N nuevos» con confirmación: uno falla y se ve el motivo',await until(async()=>(await p.textContent('#bbody')).includes('emparejamiento rechazado')));
  const lines=fs.readFileSync('/tmp/sonos_ble_mock.log','utf8').trim().split('\n').map(l=>JSON.parse(l)).filter(x=>x.cmd==='provision');
  ok('BLE: se configuraron A101 (5), B202 (6) con la WiFi guardada',lines.some(x=>x.address.endsWith('01')&&x.id===5&&x.ssid==='Casa5G'&&x.pass_len===10)&&lines.some(x=>x.address.endsWith('02')&&x.id===6),lines);
  await p.keyboard.press('Escape');ok('Esc cierra el panel Bluetooth',await until(async()=>!(await p.$('#bdrawer.on'))));
  ok('panel sin errores de JS',errs.length===0,errs);
  // ── PWA ──
  const mf=await fetch(BASE+'/manifest.webmanifest');const mj=await mf.json();
  ok('PWA: manifiesto válido (nombre, standalone, inicio, colores, 3 iconos, atajos)',mj.name==='SonosWiFi'&&mj.display==='standalone'&&mj.start_url==='/'&&mj.theme_color==='#000000'&&mj.background_color==='#000000'&&mj.lang==='es'&&mj.icons.length>=3&&mj.icons.some(i=>i.purpose==='maskable')&&mj.shortcuts.length>=2,mj);
  const pngSize=async u=>{const b=Buffer.from(await (await fetch(BASE+u)).arrayBuffer());return b.readUInt32BE(0)===0x89504e47?[b.readUInt32BE(16),b.readUInt32BE(20),b[25]]:null;};
  ok('PWA: iconos PNG con el tamaño declarado y sin canal alfa (opacos, como pide Apple)',JSON.stringify(await pngSize('/icons/icon-180.png'))==='[180,180,2]'&&JSON.stringify(await pngSize('/icons/icon-192.png'))==='[192,192,2]'&&JSON.stringify(await pngSize('/icons/icon-512.png'))==='[512,512,2]'&&JSON.stringify(await pngSize('/icons/icon-maskable-512.png'))==='[512,512,2]');
  const sw=await (await fetch(BASE+'/sw.js')).text();
  ok('PWA: service worker (la API no se cachea, la caché se invalida por versión)',sw.includes("startsWith('/api/')")&&sw.includes('caches.open')&&!sw.includes('@BUILD@')&&/sonoswifi-[0-9a-f]{10}/.test(sw.replace("'sonoswifi-@BUILD@'","")+'')||/CACHE = 'sonoswifi-[0-9a-f]{10}'/.test(sw),sw.slice(0,200));
  for(const [u,n] of [['/','panel'],['/ble','ble'],['/calibrate','calibrar']]){
    const html=await (await fetch(BASE+u)).text();
    ok('PWA: '+n+' enlaza manifiesto, apple-touch-icon, viewport-fit y modo app',html.includes('rel="manifest" href="/manifest.webmanifest"')&&html.includes('rel="apple-touch-icon" href="/icons/icon-180.png"')&&html.includes('viewport-fit=cover')&&html.includes('apple-mobile-web-app-capable')&&html.includes("register('/sw.js')"));
  }
  // ── Accesibilidad (HIG › Accessibility) ──
  const ratio=await p.evaluate(()=>{const cs=getComputedStyle(document.documentElement),v=n=>cs.getPropertyValue(n).trim();
    const lum=h=>{h=h.replace('#','');if(h.length===3)h=h.split('').map(x=>x+x).join('');const c=[0,2,4].map(i=>parseInt(h.substr(i,2),16)/255).map(x=>x<=.03928?x/12.92:Math.pow((x+.055)/1.055,2.4));return .2126*c[0]+.7152*c[1]+.0722*c[2];};
    const cr=(a,b)=>{const x=lum(a),y=lum(b);return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);};
    return {mutBg:cr(v('--mut'),v('--bg')),mutPanel:cr(v('--mut'),v('--panel')),mutRaise:cr(v('--mut'),v('--raise')),acPanel:cr(v('--ac'),v('--panel')),fgBg:cr(v('--fg'),v('--bg')),
      ctlRaise:cr(v('--ctl'),v('--raise')),ctlPanel:cr(v('--ctl'),v('--panel')),badBg:cr(v('--bad'),v('--bg')),okBg:cr(v('--ok'),v('--bg')),warnBg:cr(v('--warn'),v('--bg'))};});
  ok('contraste del texto ≥ 4.5:1 (WCAG AA, como en la HIG) sobre todas las superficies',['mutBg','mutPanel','mutRaise','acPanel','fgBg','badBg','okBg','warnBg'].every(k=>ratio[k]>=4.5),ratio);
  ok('contraste del borde de controles (campos, interruptor, botones) ≥ 3:1',ratio.ctlRaise>=3&&ratio.ctlPanel>=3,ratio);
  const small=await p.evaluate(()=>{const out=[];for(const e of document.querySelectorAll('body *')){if(!e.offsetParent&&getComputedStyle(e).position!=='fixed')continue;
    if(![...e.childNodes].some(n=>n.nodeType===3&&n.textContent.trim()))continue;const fs=parseFloat(getComputedStyle(e).fontSize);if(fs<11)out.push(e.className+':'+e.textContent.trim().slice(0,20)+':'+fs);}return out;});
  ok('ningún texto por debajo de 11 px (mínimo de iOS)',small.length===0,small);
  ok('el texto base es 17 px y sigue el tamaño de letra del navegador (rem)',await p.evaluate(()=>getComputedStyle(document.body).fontSize==='17px'&&getComputedStyle(document.documentElement).fontSize==='17px'));
  const sb=await p.locator('#lyseg button').first().boundingBox();
  ok('controles segmentados visibles (no se pisan con los LED del vúmetro)',sb&&sb.height>=30&&sb.width>=60,sb);
  ok('el cajón cerrado es inerte y está oculto (el teclado no entra en él)',await p.evaluate(()=>document.getElementById('drawer').inert===true&&document.getElementById('bdrawer').inert===true&&document.getElementById('drawer').getAttribute('aria-hidden')==='true'));
  ok('los cajones son diálogos modales con título',await p.evaluate(()=>['drawer','bdrawer'].every(i=>{const d=document.getElementById(i);return d.getAttribute('role')==='dialog'&&d.getAttribute('aria-modal')==='true'&&document.getElementById(d.getAttribute('aria-labelledby'))!==null;})));
  await p.click('#spks .spk[data-id="2"]');await p.waitForSelector('#drawer.on');
  ok('al abrir un cajón: el foco entra en él y el resto de la página queda inerte',await until(async()=>await p.evaluate(()=>document.activeElement&&document.activeElement.id==='dclose'&&document.querySelector('main').inert&&document.querySelector('.top').inert&&!document.getElementById('drawer').inert)));
  await p.keyboard.press('Escape');
  ok('al cerrar con Esc: el foco vuelve al altavoz que lo abrió y la página deja de ser inerte',await until(async()=>await p.evaluate(()=>document.activeElement&&document.activeElement.classList.contains('spk')&&document.activeElement.dataset.id==='2'&&!document.querySelector('main').inert&&document.getElementById('drawer').inert)));
  await p.focus('.tab[data-tab="spk"]');await p.keyboard.press('ArrowRight');
  ok('pestañas con teclado: → selecciona y enfoca la siguiente (foco itinerante)',await p.evaluate(()=>document.activeElement.dataset.tab==='snd'&&document.activeElement.getAttribute('aria-selected')==='true'&&document.querySelector('.tab[data-tab="spk"]').tabIndex===-1&&!document.getElementById('p_snd').hidden));
  await p.keyboard.press('End');ok('pestañas con teclado: Fin va a la última',await p.evaluate(()=>document.activeElement.dataset.tab==='log'));
  await p.keyboard.press('Home');ok('pestañas con teclado: Inicio va a la primera',await p.evaluate(()=>document.activeElement.dataset.tab==='spk'));
  ok('pestañas enlazadas con sus paneles (aria-controls / aria-labelledby)',await p.evaluate(()=>[...document.querySelectorAll('.tab')].every(t=>{const pane=document.getElementById(t.getAttribute('aria-controls'));return pane&&pane.getAttribute('aria-labelledby')===t.id;})));
  ok('los avisos se anuncian al lector de pantalla (región en vivo) y los errores como alerta',await p.evaluate(()=>{const tb=document.getElementById('tb');toast('prueba',false);const a=tb.querySelector('.toast.bad');const r=tb.getAttribute('role')==='status'&&tb.getAttribute('aria-live')==='polite'&&a&&a.getAttribute('role')==='alert';if(a)a.remove();return r;}));
  ok('volumen maestro con texto para lector de pantalla (aria-valuetext)',await p.evaluate(()=>/\d+ %/.test(document.getElementById('mvs').getAttribute('aria-valuetext')||'')));
  // Móvil
  const m=await b.newContext({viewport:{width:390,height:844},deviceScaleFactor:2,isMobile:true,hasTouch:true});
  const q=await m.newPage();await q.goto(BASE+'/');await q.waitForSelector('#spks .spk');await sleep(800);
  ok('móvil: sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_altavoces.png'});
  ok('móvil: la navegación es una barra inferior fija con icono y una sola palabra por pestaña',await q.evaluate(()=>{const t=document.querySelector('.tabs'),r=t.getBoundingClientRect(),cs=getComputedStyle(t);
    return cs.position==='fixed'&&Math.abs(r.bottom-innerHeight)<2&&[...t.querySelectorAll('.tab')].every(b=>b.querySelector('svg')&&getComputedStyle(b.querySelector('svg')).display!=='none'&&b.textContent.trim().split(/\s+/).length===1);}));
  ok('móvil: objetivos táctiles de 44 px como mínimo (botones, segmentos, pestañas, selector, campo)',await q.evaluate(()=>['#lble','#lcal','#lyseg button','.tab','#spks .spk','#mvs'].every(s=>{const e=document.querySelector(s);const r=e.getBoundingClientRect();return r.height>=44-.5;})));
  ok('móvil: la cabecera ya no es fija (la navegación va abajo)',await q.evaluate(()=>getComputedStyle(document.querySelector('.top')).position==='static'));
  ok('móvil: ningún texto por debajo de 11 px',await q.evaluate(()=>{for(const e of document.querySelectorAll('body *')){if(!e.offsetParent&&getComputedStyle(e).position!=='fixed')continue;if(![...e.childNodes].some(n=>n.nodeType===3&&n.textContent.trim()))continue;if(parseFloat(getComputedStyle(e).fontSize)<11)return false;}return true;}));
  await q.tap('#spks .spk[data-id="0"]');await q.waitForSelector('#drawer.on');await sleep(400);
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_cajon.png'});
  ok('móvil: la hoja muestra el asa (grabber)',await q.evaluate(()=>getComputedStyle(document.querySelector('#drawer .grab')).display==='block'));
  ok('móvil: los botones de la hoja miden 44 px (cerrar, interruptor)',await q.evaluate(()=>{const x=document.getElementById('dclose').getBoundingClientRect();const sw=getComputedStyle(document.querySelector('#drawer .switch'),'::before');return x.height>=43.5&&x.width>=43.5&&sw.content!=='none';}));
  {const bx=await q.locator('#dtitle').boundingBox();const cx=bx.x+bx.width/2,cy=bx.y+bx.height/2;
   await q.mouse.move(cx,cy);await q.mouse.down();await q.mouse.move(cx,cy+60,{steps:4});await q.mouse.move(cx,cy+220,{steps:6});await q.mouse.up();
   ok('móvil: deslizar la hoja hacia abajo la cierra (como un sheet de iOS)',await until(async()=>!(await q.$('#drawer.on'))));
   await q.tap('#spks .spk[data-id="0"]');await q.waitForSelector('#drawer.on');await sleep(300);
   const bx2=await q.locator('#dtitle').boundingBox();await q.mouse.move(bx2.x+bx2.width/2,bx2.y+5);await q.mouse.down();await q.mouse.move(bx2.x+bx2.width/2,bx2.y+25,{steps:3});await q.mouse.up();await sleep(300);
   ok('móvil: un arrastre corto no la cierra (vuelve a su sitio)',!!(await q.$('#drawer.on'))&&await q.evaluate(()=>document.getElementById('drawer').style.transform===''));}
  await q.tap('#dclose');await q.click('.tab[data-tab="snd"]');await sleep(300);
  ok('móvil: Sonido sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_sonido.png'});
  await m.close();
  // /ble con Web Bluetooth simulado
  const bp=await ctx.newPage();const berr=[];bp.on('pageerror',e=>berr.push(String(e)));
  await bp.addInitScript(()=>{
    const store={ssid:'MiRed5G',id:2,rpi:''};window.__w=[];
    const mk=(k)=>({readValue:async()=>k==='id'?new DataView(new Uint8Array([store.id]).buffer):new TextEncoder().encode(k==='st'?JSON.stringify({fw:5,id:store.id,wifi:1,ip:'192.168.1.55',ch:36,rssi:-58,rpi:1,sync:1}):store[k]||''),
      writeValueWithResponse:async(v)=>{window.__w.push([k,Array.from(new Uint8Array(v.buffer||v))]);},startNotifications:async()=>{},addEventListener(){}});
    const ch={'5c0a0002':'ssid','5c0a0003':'pass','5c0a0004':'id','5c0a0005':'st','5c0a0006':'cmd','5c0a0007':'rpi'};
    navigator.bluetooth={requestDevice:async()=>({name:'SonosWiFi-A1B2',addEventListener(){},gatt:{connected:true,disconnect(){},connect:async()=>({getPrimaryService:async()=>({getCharacteristic:async(u)=>mk(ch[u.slice(0,8)])})})}})};
  });
  await bp.goto(BASE+'/ble');
  ok('BLE: pasos 2 y 3 bloqueados antes de conectar',await bp.evaluate(()=>document.getElementById('s2').classList.contains('lock')&&document.getElementById('s3').classList.contains('lock')));
  if(SHOTS)await bp.screenshot({path:SHOTS+'/ble_inicio.png',fullPage:true});
  await bp.click('#bc');await bp.waitForSelector('#s3:not(.lock)');
  ok('BLE: estado legible tras conectar',(await bp.textContent('#k_wifi'))==='Conectado a la WiFi'&&(await bp.textContent('#k_rssi')).startsWith('Buena')&&(await bp.textContent('#k_ip'))==='192.168.1.55'&&(await bp.textContent('#k_rpi'))==='Sincronizado');
  ok('BLE: lee ID y SSID guardados',(await bp.inputValue('#ssid'))==='MiRed5G'&&(await bp.getAttribute('#pills button[data-i="2"]','aria-pressed'))==='true');
  await bp.click('#pills button[data-i="7"]');await bp.fill('#ssid','Casa5G');await bp.fill('#pass','clave1234');
  if(SHOTS)await bp.screenshot({path:SHOTS+'/ble_conectado.png',fullPage:true});
  await bp.click('#save');await bp.waitForFunction(()=>window.__w.some(w=>w[0]==='cmd'));
  const w=await bp.evaluate(()=>window.__w);const dec=a=>String.fromCharCode(...a);
  ok('BLE: escribe ID 7, SSID, contraseña y «apply»',JSON.stringify(w.find(x=>x[0]==='id')[1])==='[7]'&&dec(w.find(x=>x[0]==='ssid')[1])==='Casa5G'&&dec(w.find(x=>x[0]==='pass')[1])==='clave1234'&&dec(w.find(x=>x[0]==='cmd')[1])==='apply',w);
  await bp.fill('#pass','corta');await bp.click('#save');
  ok('BLE: valida la contraseña corta',(await bp.textContent('#res')).includes('8 y 63'));
  ok('BLE sin errores de JS',berr.length===0,berr);
  // ── Tema claro / oscuro ──
  const lum=h=>{const m=h.match(/\d+/g).map(Number),f=v=>{v/=255;return v<=.03928?v/12.92:Math.pow((v+.055)/1.055,2.4);};return .2126*f(m[0])+.7152*f(m[1])+.0722*f(m[2]);};
  const cr=(a,b)=>{const x=lum(a),y=lum(b);return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);};
  const colors=()=>p.evaluate(()=>{const g=(el,pr)=>getComputedStyle(el)[pr];return {bg:g(document.body,'backgroundColor'),fg:g(document.body,'color'),
    mut:g(document.querySelector('.hint'),'color'),panel:g(document.getElementById('room'),'backgroundColor'),
    btn:g(document.querySelector('.btn:not(.ghost)')||document.body,'color'),theme:document.documentElement.dataset.theme,
    meta:document.querySelector('meta[name=theme-color]').content,cs:getComputedStyle(document.documentElement).colorScheme};});
  await p.evaluate(()=>localStorage.removeItem('sonos_theme'));await p.reload();await p.waitForSelector('.spk');
  let c=await colors();
  ok('Tema: por defecto oscuro (fondo negro, theme-color #000000)',c.theme==='dark'&&c.bg==='rgb(0, 0, 0)'&&c.meta==='#000000'&&c.cs==='dark',c);
  ok('Tema: el botón existe y dice Oscuro',(await p.textContent('[data-theme-btn]')).includes('Oscuro'));
  await p.click('[data-theme-btn]');c=await colors();
  ok('Tema: un toque pasa a claro (fondo claro, texto oscuro, theme-color claro)',c.theme==='light'&&lum(c.bg)>.8&&lum(c.fg)<.1&&c.meta==='#f3f5f9'&&c.cs==='light',c);
  ok('Tema claro: contraste texto/fondo ≥ 4,5 y texto atenuado ≥ 4,5',cr(c.fg,c.bg)>=4.5&&cr(c.mut,c.bg)>=4.5,[cr(c.fg,c.bg),cr(c.mut,c.bg)]);
  ok('Tema claro: texto de botón primario sobre acento ≥ 4,5',await p.evaluate(()=>{const b=document.createElement('button');b.className='btn';document.body.appendChild(b);const s=getComputedStyle(b);const r=[s.color,s.backgroundColor];b.remove();return r;}).then(r=>cr(r[0],r[1])>=4.5));
  ok('Tema claro: el acento (azul) sobre el fondo claro ≥ 4,5',await p.evaluate(()=>getComputedStyle(document.documentElement).getPropertyValue('--ac').trim()).then(v=>{const h=v.replace('#','');return cr('rgb('+[0,2,4].map(i=>parseInt(h.substr(i,2),16)).join(',')+')',c.bg)>=4.5;}));
  ok('Tema claro: sin morado en los acentos',await p.evaluate(()=>['--ac','--bad','--ok','--warn'].every(k=>{const h=getComputedStyle(document.documentElement).getPropertyValue(k).trim().replace('#','');const r=parseInt(h.substr(0,2),16),g=parseInt(h.substr(2,2),16),b=parseInt(h.substr(4,2),16);return !(b>r&&r>g&&b>150&&r>100);})));
  ok('Tema: guardado en localStorage',await p.evaluate(()=>localStorage.getItem('sonos_theme'))==='light');
  if(SHOTS){await p.screenshot({path:SHOTS+'/tema_claro_panel.png',fullPage:false});}
  await p.reload();await p.waitForSelector('.spk');c=await colors();
  ok('Tema: persiste al recargar y sin destello (ya claro al cargar)',c.theme==='light');
  await p.emulateMedia({colorScheme:'dark'});
  await p.click('[data-theme-btn]');c=await colors();
  ok('Tema: tercer toque = Automático; sigue al sistema (aquí oscuro)',(await p.textContent('[data-theme-btn]')).includes('Automático')&&c.theme==='dark',c);
  await p.emulateMedia({colorScheme:'light'});await sleep(200);c=await colors();
  ok('Tema automático: cambia a claro cuando el sistema pasa a claro',c.theme==='light',c);
  await p.emulateMedia({colorScheme:'dark'});await sleep(200);c=await colors();
  ok('Tema automático: vuelve a oscuro',c.theme==='dark',c);
  await p.click('[data-theme-btn]');ok('Tema: cuarto toque vuelve a oscuro',(await colors()).theme==='dark');
  await p.evaluate(()=>localStorage.setItem('sonos_theme','light'));
  for(const u of ['/calibrate','/ble']){const q=await ctx.newPage();await q.goto(BASE+u);ok('Tema claro compartido en '+u,await q.evaluate(()=>document.documentElement.dataset.theme)==='light'&&await q.locator('[data-theme-btn]').count()===1);await q.close();}
  await p.evaluate(()=>localStorage.removeItem('sonos_theme'));await p.reload();await p.waitForSelector('.spk');

  // ── Plano de la sala: mover altavoces ──
  const spkPos=async id=>p.evaluate(i=>{const e=document.querySelector('#spks .spk[data-id="'+i+'"]'),r=document.getElementById('room').getBoundingClientRect(),b=e.getBoundingClientRect();return [ (b.left+b.width/2-r.left)/r.width*100,(b.top+b.height/2-r.top)/r.height*100 ];},id);
  ok('Plano: etiquetas Pantalla/Atrás/Escucha',(await p.textContent('.screen'))==='Pantalla'&&await p.locator('.rl.back').textContent()==='Atrás'&&(await p.textContent('.me small'))==='Escucha');
  ok('Plano: fuera del modo mover, tocar un altavoz abre el cajón',await (async()=>{await p.click('#spks .spk[data-id="0"]');const o=await p.evaluate(()=>document.getElementById('drawer').classList.contains('on'));await p.click('#drawer .x');return o;})());
  const before=await spkPos(0);const gd0=await p.locator('.gd').count();const vol00=(await J('/api/nodes'))[0].volume_l;
  await p.click('#medit');
  ok('Plano: «Mover altavoces» activa el modo (aria-pressed, botón Listo, restablecer visible)',await p.getAttribute('#medit','aria-pressed')==='true'&&(await p.textContent('#medit'))==='Listo'&&await p.isVisible('#mreset'));
  await p.click('#spks .spk[data-id="0"]',{force:true});
  ok('Plano: en modo mover, tocar no abre el cajón',!(await p.evaluate(()=>document.getElementById('drawer').classList.contains('on'))));
  const rb=await p.locator('#room').boundingBox();
  const b0=await p.locator('#spks .spk[data-id="0"] .orb').boundingBox();
  const sx=b0.x+b0.width/2,sy=b0.y+b0.height/2,tx=rb.x+rb.width*0.30,ty=rb.y+rb.height*0.55;
  await p.mouse.move(sx,sy);await p.mouse.down();await p.mouse.move((sx+tx)/2,(sy+ty)/2,{steps:6});await p.mouse.move(tx,ty,{steps:6});await p.mouse.up();
  await until(async()=>(await J('/api/nodes'))[0].pos_x>=0);
  let n0=(await J('/api/nodes'))[0],after=await spkPos(0);
  ok('Plano: arrastrar guarda la posición en la Raspberry (≈30 %, 55 %)',Math.abs(n0.pos_x-30)<2&&Math.abs(n0.pos_y-55)<2,[n0.pos_x,n0.pos_y]);
  ok('Plano: el altavoz queda donde se soltó',Math.abs(after[0]-30)<2&&Math.abs(after[1]-55)<2&&(Math.abs(after[0]-before[0])>5),[before,after]);
  ok('Plano: arrastrar no cambia canales ni volumen',n0.ch_left===0&&n0.ch_right===1&&n0.volume_l===vol00&&n0.volume_r===255,[n0,vol00]);
  ok('Plano: ya no se dibujan los canales fantasma de ese altavoz',await until(async()=>await p.locator('.gd').count()===gd0-2));
  // límites: soltar fuera del plano se queda dentro
  await p.waitForSelector('#spks .spk[data-id="0"] .orb');const b1=await p.locator('#spks .spk[data-id="0"] .orb').boundingBox();
  await p.mouse.move(b1.x+b1.width/2,b1.y+b1.height/2);await p.mouse.down();await p.mouse.move(rb.x-300,rb.y-300,{steps:8});await p.mouse.up();
  await until(async()=>(await J('/api/nodes'))[0].pos_x<10);
  n0=(await J('/api/nodes'))[0];
  ok('Plano: arrastrar fuera del plano se limita al borde (4–96 %, 7–93 %)',n0.pos_x>=3.9&&n0.pos_x<=96&&n0.pos_y>=6.9&&n0.pos_y<=93,[n0.pos_x,n0.pos_y]);
  // teclado
  await p.focus('#spks .spk[data-id="1"]');await p.keyboard.press('ArrowRight');await p.keyboard.press('ArrowDown');await p.keyboard.press('Shift+ArrowDown');
  const k1=await until(async()=>{const n=(await J('/api/nodes'))[1];return n.pos_x>=0;},3000);
  let n1=(await J('/api/nodes'))[1];const p1=await spkPos(1);
  ok('Plano: las flechas del teclado mueven el altavoz y se guarda (y el foco se mantiene)',k1&&await p.evaluate(()=>document.activeElement.dataset.id)==='1'&&Math.abs(n1.pos_x-p1[0])<1.5&&Math.abs(n1.pos_y-p1[1])<1.5,[n1.pos_x,n1.pos_y,p1]);
  if(SHOTS)await p.screenshot({path:SHOTS+'/plano_mover.png',fullPage:true});
  // recarga: la posición persiste
  await p.reload();await p.waitForSelector('.spk');
  const rl=await spkPos(0);ok('Plano: tras recargar el altavoz sigue en su sitio',Math.abs(rl[0]-n0.pos_x)<1.5&&Math.abs(rl[1]-n0.pos_y)<1.5,[rl,n0.pos_x,n0.pos_y]);
  // restablecer
  await p.click('#medit');await p.click('#mreset');
  await until(async()=>(await J('/api/nodes')).every(n=>n.pos_x<0));
  ok('Plano: «Restablecer posiciones» devuelve todos al lugar de su canal',(await J('/api/nodes')).every(n=>n.pos_x<0&&n.pos_y<0));
  await p.click('#medit');
  ok('Plano: «Listo» sale del modo mover',await p.getAttribute('#medit','aria-pressed')==='false'&&!(await p.isVisible('#mreset')));
  // móvil: arrastre táctil (modo mover) y tema claro sin desbordes
  const ctx2=await b.newContext({viewport:{width:390,height:844},deviceScaleFactor:2,isMobile:true,hasTouch:true});
  const mp=await ctx2.newPage();await mp.goto(BASE+'/');await mp.waitForSelector('.spk');
  await mp.evaluate(()=>localStorage.setItem('sonos_theme','light'));await mp.reload();await mp.waitForSelector('.spk');
  ok('Móvil: tema claro sin desbordamiento horizontal',await mp.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  await mp.click('#medit');
  await mp.evaluate(()=>window.scrollTo(0,document.getElementById('room').getBoundingClientRect().top+scrollY-70));await sleep(200);const sy0=await mp.evaluate(()=>scrollY);
  const mrb=await mp.locator('#room').boundingBox(),mb0=await mp.locator('#spks .spk[data-id="2"] .orb').boundingBox();
  const cdp=await ctx2.newCDPSession(mp);const sx2=mb0.x+mb0.width/2,sy2=mb0.y+mb0.height/2,tx2=mrb.x+mrb.width*0.7,ty2=mrb.y+mrb.height*0.45;
  const tp=(t,x,y)=>cdp.send('Input.dispatchTouchEvent',{type:t,touchPoints:t==='touchEnd'?[]:[{x,y,id:1}]});
  await tp('touchStart',sx2,sy2);for(let i=1;i<=8;i++)await tp('touchMove',sx2+(tx2-sx2)*i/8,sy2+(ty2-sy2)*i/8);await tp('touchEnd');
  const okT=await until(async()=>(await J('/api/nodes'))[2].pos_x>=0,3000);const m2=(await J('/api/nodes'))[2];
  ok('Móvil: arrastre táctil mueve el altavoz (≈70 %, 45 %) sin desplazar la página',okT&&Math.abs(m2.pos_x-70)<4&&Math.abs(m2.pos_y-45)<4&&await mp.evaluate(()=>scrollY)===sy0,[m2.pos_x,m2.pos_y,sy0]);
  if(SHOTS)await mp.screenshot({path:SHOTS+'/movil_claro_plano.png'});
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:2,pos_auto:true})});
  await mp.close();
  ok('Panel sin errores de JS (tema y plano)',errs.length===0,errs);

  // /calibrate con resultado inyectado
  const cp=await ctx.newPage();const cerr=[];cp.on('pageerror',e=>cerr.push(String(e)));
  await cp.goto(BASE+'/calibrate');
  if(SHOTS)await cp.screenshot({path:SHOTS+'/calibrar_inicio.png',fullPage:true});
  const nodes=[0,1,2,3,4].map(i=>({id:i,d_us:2000+i*700,spread_us:60+i*20,level_db:-30+i*1.5,snr:25-i,ok:i!==3,bands:[0,1,2,3,4,5].map(k=>-40+(i-2)*(k-2.5)*0.8+(i%2?1.5:-1.5)),band_ok:[1,1,1,1,1,1].map(()=>true)}));
  await cp.evaluate(ns=>{lastRes={nodes:ns,noise_db:-60};eqOn=false;info={0:{ch_left:0,ch_right:1,mono:false,role:0},4:{ch_left:3,ch_right:3,mono:true,role:2}};render();view(3);},nodes);
  ok('Calibrar: 5 tarjetas de resultado',await cp.locator('#cards .node').count()===5);
  ok('Calibrar: tarjeta débil marcada',await cp.locator('#cards .node.bad').count()===1);
  ok('Calibrar: aviso de EQ en pass-through visible',!(await cp.getAttribute('#eqoff','hidden')!==null));
  if(SHOTS)await cp.screenshot({path:SHOTS+'/calibrar_resultado.png',fullPage:true});
  const vol0=(await J('/api/nodes'))[0].volume_l;
  await cp.uncheck('#ck_vol');
  await cp.click('#apply');await cp.waitForFunction(()=>document.getElementById('applied').textContent.includes('Aplicado'));
  const nn=await J('/api/nodes'),st=await J('/api/status');
  ok('Calibrar: aplica latencias (nodo 0 recibe la mayor)',nn[0].latency_offset_us>nn[4].latency_offset_us&&nn[4].latency_offset_us===0,nn.map(x=>x.latency_offset_us));
  ok('Calibrar: aplica EQ y activa el ecualizador',nn.some(x=>x.eq.some(g=>Math.abs(g)>0.05))&&st.dsp_eq===true,st);
  ok('Calibrar: no toca volúmenes si está desmarcado',nn[0].volume_l===vol0,[nn[0].volume_l,vol0]);
  await cp.click('#reset');cp.once('dialog',()=>{});
  ok('Calibrar sin errores de JS',cerr.length===0,cerr);
  await fetch(BASE+'/api/dsp',{method:'POST',body:JSON.stringify({xover:false,eq:false})});
  for(let i=0;i<10;i++){await fetch(BASE+'/api/calibrate',{method:'POST',body:JSON.stringify({esp_id:i,latency_us:0})});await fetch(BASE+'/api/eq',{method:'POST',body:JSON.stringify({id:i,reset:true})});}
  await b.close();
  console.log(fails?`RESULTADO UI: ${fails} FALLOS`:'RESULTADO UI: TODO OK');process.exit(fails?1:0);
})().catch(e=>{console.error('FAIL UI excepción',e);process.exit(1);});
