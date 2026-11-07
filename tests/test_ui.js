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
  const n=await p.locator('#spks .spk').count();ok('plano: un altavoz dibujado por canal (5 ESP estéreo = 10)',n===10,n);
  ok('plano: nodos en línea (punto verde)',await p.locator('#spks .st.on').count()===10);
  ok('estado: «5 de 5 en línea»',(await p.textContent('#onl')).includes('5 de 5'));
  ok('sin selector «5 estéreo / 10 mono» (cada ESP elige estéreo o mono en su cajón)',await p.locator('#lyseg').count()===0&&!(await p.textContent('body')).includes('10 mono'));
  // solo existen en el plano los ESP que se conectaron alguna vez (known), aunque haya otros «habilitados»
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:8,enabled:true})});await sleep(1500);
  ok('plano: un ESP habilitado que nunca se conectó (8) no aparece ni en el plano ni en la lista',await p.locator('#spks .spk[data-id="8"]').count()===0&&await p.locator('#chips button[data-id="8"]').count()===0&&(await J('/api/nodes'))[8].known===false);
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:8,enabled:false})});
  // mezcla: 1 ESP estéreo y 2 ESP mono (antes solo había «5 estéreo» o «10 mono»)
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:1,mono:true,ch_left:2,volume_l:255})});
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:2,mono:true,ch_left:4,volume_l:255})});
  await p.reload();await p.waitForSelector('#spks .spk');await sleep(1200);
  const mix=await p.evaluate(()=>[...document.querySelectorAll('#spks .spk')].reduce((a,e)=>{a[e.dataset.id]=(a[e.dataset.id]||0)+1;return a;},{}));
  ok('plano: ESP 0 estéreo = 2 altavoces; ESP 1 y 2 mono = 1 cada uno',mix[0]===2&&mix[1]===1&&mix[2]===1&&mix[3]===2&&mix[4]===2,mix);
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:1,mono:false,ch_left:2,ch_right:3})});
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:2,mono:false,ch_left:4,ch_right:5})});
  await fetch(BASE+'/api/layout',{method:'POST',body:JSON.stringify({layout:'stereo5'})});
  await p.reload();await p.waitForSelector('#spks .spk');await sleep(800);
  // quitar un ESP del panel: no se puede con uno en línea
  await p.click('#spks .spk[data-id="3"][data-side="0"]');
  ok('cajón: «Quitar este ESP del panel» está desactivado mientras el ESP está en línea',await p.isDisabled('#d_fg'));
  await p.click('#drawer .x');
  // Estado de transmisión y vúmetros (datos reales del arnés: ch0/1 −6 dBFS, ch2 y ch4 −12 dBFS, el resto en silencio)
  ok('estado: «Transmitiendo · 5 altavoces»',await until(async()=>(await p.textContent('#hs')).includes('Transmitiendo · 5 altavoces')),await p.textContent('#hs'));
  ok('estado: punto verde',(await p.getAttribute('#dot','class')).includes('ok'));
  // Fila superior: un cuadrado por canal (rojo y parpadeando si hay señal, apagado si no) con el nivel debajo
  const lamps=await p.evaluate(()=>[...document.querySelectorAll('#mb .mc')].map(m=>({on:m.querySelector('.lamp').classList.contains('on'),num:m.querySelector('.lvn').textContent})));
  ok('vúmetros: 10 cuadrados indicadores, uno por canal',lamps.length===10);
  ok('vúmetros: encendidos solo FL, FR, C y canal 4 (los que tienen señal); el resto apagado',lamps.map(l=>+l.on).join('')==='1110100000',lamps);
  ok('vúmetros: el número bajo cada cuadrado es el nivel (−6, −6, −12, —, −12)',lamps.slice(0,5).map(l=>l.num).join('|')==='−6|−6|−12|—|−12',lamps);
  const lp=await p.evaluate(()=>{const on=getComputedStyle(document.getElementById('lp0')),off=getComputedStyle(document.getElementById('lp3'));
    return {onBg:on.backgroundColor,offBg:off.backgroundColor,anim:on.animationName,animOff:off.animationName,r:on.borderRadius,w:on.width,h:on.height};});
  ok('vúmetros: cuadrado rojo con parpadeo si hay señal; apagado y quieto si no',lp.onBg==='rgb(255, 69, 58)'&&lp.anim==='lampblink'&&lp.offBg!==lp.onBg&&lp.animOff==='none'&&lp.w===lp.h,lp);
  ok('plano: ya no está la barra de 20 LED por canal en la tarjeta Entrada',(await p.locator('#mb .bar').count())===0);
  // Vúmetro junto a cada altavoz, con la misma altura que el dibujo
  const vu=await p.evaluate(()=>[...document.querySelectorAll('#spks .spk')].map(b=>{const c=b.querySelector('.cab').getBoundingClientRect(),v=b.querySelector('.vu').getBoundingClientRect();
    return {ch:b.querySelector('.ch').textContent,dh:Math.abs(c.height-v.height),side:v.left>=c.right,lit:b.querySelectorAll('.vu .led.on').length,n:b.querySelectorAll('.vu .led').length};}));
  ok('plano: cada altavoz tiene su vúmetro a un lado',vu.length>=5&&vu.every(x=>x.side&&x.n===12),vu);
  ok('plano: el vúmetro mide lo mismo que el altavoz (±1 px)',vu.every(x=>x.dh<=1),vu);
  const vl=Object.fromEntries(vu.map(x=>[x.ch,x.lit]));
  ok('plano: FL/FR (−6 dBFS) ≈ 11 LED, C (−12) ≈ 9 y los canales en silencio, apagados',vl.FL>=10&&vl.FL<=12&&vl.FR>=10&&vl.FR<=12&&vl.C>=8&&vl.C<=10&&vl.LFE===0,vl);
  const cs=await p.evaluate(()=>{const v=document.querySelector('#spks .vu');return {cls:[...v.children].map(x=>x.className.split(' ')[1]).join(''),
      col:[...v.children].filter((x,i)=>[0,8,11].includes(i)).map(x=>getComputedStyle(x).backgroundColor),bg:getComputedStyle(v.children[0]).backgroundImage};});
  ok('plano: vúmetro de 12 LED: 8 verdes (hasta −20 dB), 2 amarillos (hasta −10 dB), 2 rojos',cs.cls==='g'.repeat(8)+'y'.repeat(2)+'r'.repeat(2),cs.cls);
  if(SHOTS){await p.locator('#meters').screenshot({path:SHOTS+'/entrada_leds.png'});await p.locator('#room').screenshot({path:SHOTS+'/plano_vumetros.png'});}
  ok('plano: colores estándar verde/amarillo/rojo y planos (sin degradado)',cs.col.join('|')==='rgb(48, 209, 88)|rgb(255, 214, 10)|rgb(255, 69, 58)'&&cs.bg==='none',cs);
  ok('vúmetros: texto de pico ≈ −6 dBFS',(await p.textContent('#mtxt')).includes('−')===false&&(await p.textContent('#mtxt')).includes('-6'),await p.textContent('#mtxt'));
  const fake=async(o)=>{await p.route('**/api/levels',r=>r.fulfill({contentType:'application/json',body:JSON.stringify(Object.assign({capturing:true,signal:true,signal_age_ms:10,pps:900,sending:true,nodes_online:5,db:[-6,-6,-12,-90,-12,-90,-90,-90,-90,-90]},o))}));};
  await fake({capturing:false,signal:false,sending:false,db:[-90,-90,-90,-90,-90,-90,-90,-90,-90,-90]});
  ok('estado: sin captura -> «Sin captura de la Traktor» (rojo) y todo apagado',await until(async()=>(await p.textContent('#hs'))==='Sin captura de la Traktor'&&(await p.getAttribute('#dot','class')).includes('bad')&&(await p.getAttribute('#meters','class')).includes('off')&&(await p.locator('#mb .lamp.on').count())===0&&(await p.locator('#spks .vu .led.on').count())===0));
  await fake({signal:false,sending:false,db:[-90,-90,-90,-90,-90,-90,-90,-90,-90,-90]});
  ok('estado: silencio -> «Traktor conectada, sin señal» (ámbar)',await until(async()=>(await p.textContent('#hs'))==='Traktor conectada, sin señal'&&(await p.getAttribute('#dot','class')).includes('warn')&&(await p.textContent('#mtxt'))==='Silencio'));
  await fake({sending:false,nodes_online:0});
  ok('estado: señal sin altavoces -> «Hay señal, ningún altavoz en línea»',await until(async()=>(await p.textContent('#hs'))==='Hay señal, ningún altavoz en línea'));
  await fake({db:[-0.5,-6,-12,-90,-12,-90,-90,-90,-90,-90]});
  ok('vúmetros: clip (≥ −1 dBFS) muestra «0» bajo el cuadrado y los 2 LED rojos del altavoz FL',await until(async()=>(await p.textContent('#nm0'))==='0'&&await p.evaluate(()=>{const b=[...document.querySelectorAll('#spks .spk')].find(x=>x.querySelector('.ch').textContent==='FL');return !!b&&b.querySelectorAll('.vu .led.r.on').length===2;})));
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
  await p.click('#tab_lat');const sb=await p.locator('#plseg button').first().boundingBox();await p.click('#tab_spk');
  ok('controles segmentados visibles (no se pisan con los indicadores de señal)',sb&&sb.height>=30&&sb.width>=60,sb);
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
  // ── Funciones nuevas: probar altavoz, distancias, modo noche, LFE, perfiles, salud ──
  await p.click('[data-tab=spk]');await p.click('#spks .spk[data-id="3"][data-side="0"]');await p.waitForSelector('#drawer.on');await sleep(300);
  ok('cajón: ESP estéreo ofrece Izquierdo, Derecho y Ambos para probar',await p.locator('#d_tone button').count()===3);
  await p.click('#d_tone button[data-side="2"]');
  ok('probar: «Ambos» inicia el tono en el servidor y el botón queda pulsado',await until(async()=>{const t=await J('/api/tone');return t.active&&t.id===3;})&&await until(async()=>(await p.getAttribute('#d_tone button[data-side="2"]','aria-pressed'))==='true'),[await J('/api/tone'),await p.textContent('#d_toneh'),await J('/api/cal/status')]);
  await p.click('#d_tone button[data-side="2"]');
  ok('probar: pulsar otra vez lo para',await until(async()=>!(await J('/api/tone')).active));
  await p.fill('#d_dist','2');await p.keyboard.press('Tab');
  ok('distancia: se guarda sola desde el cajón (2 m)',await until(async()=>(await J('/api/nodes'))[3].dist_m===2));
  await p.fill('#d_dist','');await p.keyboard.press('Tab');
  ok('distancia: vaciar el campo la borra',await until(async()=>(await J('/api/nodes'))[3].dist_m===-1));
  await p.fill('#d_dist','2');await p.keyboard.press('Tab');await sleep(900);
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_cajon_probar.png'});
  await p.keyboard.press('Escape');await until(async()=>!(await p.$('#drawer.on')));
  await p.click('#spks .spk[data-id="4"][data-side="0"]');await p.waitForSelector('#drawer.on');await sleep(300);
  await p.fill('#d_dist','3.5');await p.keyboard.press('Tab');
  ok('distancia: segundo altavoz a 3,5 m',await until(async()=>(await J('/api/nodes'))[4].dist_m===3.5));
  await p.keyboard.press('Escape');await until(async()=>!(await p.$('#drawer.on')));
  await p.click('.tab[data-tab="lat"]');await sleep(600);
  ok('latencia: con dos distancias aparece la tabla de cálculo y «Aplicar» se activa',await p.locator('#distw tbody tr').count()===2&&!(await p.isDisabled('#distapply')));
  const dtxt=await p.textContent('#distw');
  ok('latencia: el más lejano (3,5 m) sin retardo y el de 2 m con 4,4 ms',dtxt.includes('3,5 m')&&dtxt.includes('4,4 ms')&&dtxt.includes('57%'),dtxt);
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_latencia_distancias.png',fullPage:true});
  await p.click('#distapply');
  ok('distancias: «Aplicar» fija retardo 4373 µs y volumen 146 en el altavoz de 2 m',await until(async()=>{const x=(await J('/api/nodes'))[3];return Math.abs(x.latency_offset_us-4373)<=1&&x.volume_l===146&&x.volume_r===146;}),(await J('/api/nodes'))[3]);
  ok('distancias: el más lejano queda a 255 y 0 µs',await until(async()=>{const x=(await J('/api/nodes'))[4];return x.latency_offset_us===0&&x.volume_l===255;}));
  ok('salud: la tabla tiene columnas WiFi y Firmware y 11 columnas por fila',await p.evaluate(()=>[...document.querySelectorAll('#health tr')].every(r=>r.children.length===11)&&[...document.querySelectorAll('thead th')].some(t=>t.textContent==='WiFi')&&[...document.querySelectorAll('thead th')].some(t=>t.textContent==='Firmware')));
  ok('aviso de desconexión: un altavoz que pasa de en línea a desconectado muestra un aviso',await p.evaluate(()=>{wasOn[3]=true;nodes=nodes.map(n=>n.id===3?{...n,online:false,enabled:true}:n);watchOnline();return [...document.querySelectorAll('#tb .toast.bad')].some(t=>t.textContent.includes('#3')&&t.textContent.includes('se desconectó'));}));
  await p.evaluate(()=>document.querySelectorAll('#tb .toast').forEach(t=>t.remove()));
  await p.click('.tab[data-tab="snd"]');await sleep(300);
  await p.click('#ntseg button[data-v="2"]');
  ok('modo noche: «Medio» llega al servidor y queda marcado',await until(async()=>(await J('/api/status')).night===2)&&await until(async()=>(await p.getAttribute('#ntseg button[data-v="2"]','aria-pressed'))==='true'));
  ok('modo noche: el texto explica el nivel elegido',(await p.textContent('#nth')).startsWith('Medio'));
  await p.click('#sw_lfe');
  ok('LFE en frontales: el interruptor llega al servidor',await until(async()=>(await J('/api/status')).lfe_front===true));
  await p.evaluate(()=>{const e=document.getElementById('lfes');e.value=4;e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));});
  ok('LFE: el nivel +4 dB llega al servidor',await until(async()=>(await J('/api/status')).lfe_db===4));
  await p.fill('#pfname','Cine noche');await p.click('#pfsave');
  ok('perfiles: guardar muestra el perfil en la lista',await until(async()=>(await p.locator('#pflist .pfrow').count())===1&&(await p.textContent('#pflist')).includes('Cine noche')));
  await p.click('#ntseg button[data-v="0"]');await p.click('#sw_lfe');
  await until(async()=>{const s=await J('/api/status');return s.night===0&&s.lfe_front===false;});
  await p.click('#pflist [data-load="0"]');
  ok('perfiles: cargar restaura modo noche 2 y LFE activado, y lo marca «En uso»',await until(async()=>{const s=await J('/api/status');return s.night===2&&s.lfe_front===true&&s.lfe_db===4;})&&await until(async()=>(await p.textContent('#pflist')).includes('En uso')));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_sonido_nuevo.png',fullPage:true});
  await p.click('#pflist [data-del="0"]');
  ok('perfiles: borrar pide confirmar con una segunda pulsación',(await p.locator('#pflist .pfrow').count())===1&&(await p.textContent('#pflist [data-del="0"]')).includes('Seguro'));
  await p.click('#pflist [data-del="0"]');
  ok('perfiles: segunda pulsación lo borra',await until(async()=>(await p.locator('#pflist .pfrow').count())===0&&(await J('/api/profiles')).profiles.length===0));
  await p.click('#pfsave');
  ok('perfiles: guardar sin nombre avisa y no crea nada',(await J('/api/profiles')).profiles.length===0&&await p.locator('#tb .toast.bad').count()>=1);
  await p.evaluate(()=>document.querySelectorAll('#tb .toast').forEach(t=>t.remove()));
  await fetch(BASE+'/api/dsp',{method:'POST',body:JSON.stringify({night:0,lfe:false,lfe_db:0})});
  for(const i of [3,4]){await fetch(BASE+'/api/calibrate',{method:'POST',body:JSON.stringify({esp_id:i,latency_us:0})});await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:i,volume_l:255,volume_r:255,dist_m:-1})});}
  // Actualización de firmware por WiFi: tarjeta en Latencia (el arnés simula ESP con firmware antiguo, sin versión)
  {const fsx=require('fs');const img=Buffer.alloc(80000,5);img[0]=0xE9;img[1]=1;img.writeUInt16LE(0x17,12);img.writeUInt32LE(0xABCD5432,32);img.write('9.9-ui',48);
   fsx.writeFileSync('/tmp/fw_ui_bad.bin',Buffer.alloc(70000,7));fsx.writeFileSync('/tmp/fw_ui_ok.bin',img);
   await p.click('.tab[data-tab="lat"]');await sleep(300);
   ok('firmware: aviso de «sin probar en hardware» y de flashear la primera vez por cable',(await p.textContent('#fwwarn')).includes('sin probar')&&(await p.textContent('#fwwarn')).includes('por cable'));
   await p.click('#fwup');
   ok('firmware: subir sin elegir archivo avisa',await p.locator('#tb .toast.bad').count()>=1);
   await p.evaluate(()=>document.querySelectorAll('#tb .toast').forEach(t=>t.remove()));
   await p.setInputFiles('#fwfile','/tmp/fw_ui_bad.bin');await p.click('#fwup');
   ok('firmware: un archivo que no es firmware se rechaza con un mensaje claro',await until(async()=>(await p.locator('#tb .toast.bad').allTextContents()).some(t=>t.includes('ESP-IDF'))));
   await p.evaluate(()=>document.querySelectorAll('#tb .toast').forEach(t=>t.remove()));
   await p.setInputFiles('#fwfile','/tmp/fw_ui_ok.bin');await p.click('#fwup');
   ok('firmware: un .bin válido se carga y se muestra versión, tamaño y chip',await until(async()=>{const t=await p.textContent('#fwinfo');return t.includes('9.9-ui')&&t.includes('78 KB')&&t.includes('C5');}),await p.textContent('#fwinfo'));
   ok('firmware: los ESP sin versión no se pueden actualizar por WiFi (botón desactivado y motivo visible)',await p.evaluate(()=>{const b=[...document.querySelectorAll('#fwlist [data-ota]')];return b.length===5&&b.every(x=>x.disabled)&&document.getElementById('fwlist').textContent.includes('primero por cable');}));
   if(SHOTS)await p.screenshot({path:SHOTS+'/panel_firmware.png',fullPage:true});}
  ok('nuevas funciones: sin errores de JS',errs.length===0,errs);
  await p.click('.tab[data-tab="spk"]');
  // Móvil
  const m=await b.newContext({viewport:{width:390,height:844},deviceScaleFactor:2,isMobile:true,hasTouch:true});
  const q=await m.newPage();await q.goto(BASE+'/');await q.waitForSelector('#spks .spk');await sleep(800);
  ok('móvil: sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  if(SHOTS){await q.screenshot({path:SHOTS+'/movil_altavoces.png'});await q.locator('#meters').screenshot({path:SHOTS+'/movil_entrada_leds.png'});await q.locator('#room').scrollIntoViewIfNeeded();await q.locator('#room').screenshot({path:SHOTS+'/movil_plano_vumetros.png'});}
  ok('móvil: la navegación es una píldora flotante abajo (separada de los bordes) con icono y una sola palabra por pestaña',await q.evaluate(()=>{const t=document.querySelector('.tabs'),r=t.getBoundingClientRect(),cs=getComputedStyle(t);
    return cs.position==='fixed'&&r.bottom<=innerHeight-8&&r.left>=8&&r.right<=innerWidth-8&&parseFloat(cs.borderTopLeftRadius)>=24&&[...t.querySelectorAll('.tab')].every(b=>b.querySelector('svg')&&getComputedStyle(b.querySelector('svg')).display!=='none'&&b.textContent.trim().split(/\s+/).length===1);}));
  ok('móvil: objetivos táctiles de 44 px como mínimo (botones, segmentos, pestañas, selector, campo)',await q.evaluate(()=>['#lble','#lcal','.tab','#spks .spk','#mvs'].every(s=>{const e=document.querySelector(s);const r=e.getBoundingClientRect();return r.height>=44-.5;})));
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
  await q.click('.tab[data-tab="snd"]');await q.evaluate(()=>window.scrollTo(0,document.getElementById('c_nt').getBoundingClientRect().top+scrollY-70));await sleep(300);
  ok('móvil: modo noche, LFE y perfiles sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  ok('móvil: los segmentos de modo noche miden 44 px',await q.evaluate(()=>[...document.querySelectorAll('#ntseg button')].every(b=>b.getBoundingClientRect().height>=43.5)));
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_sonido_nuevo.png'});
  await q.tap('.tab[data-tab="spk"]');await q.tap('#spks .spk[data-id="0"]');await q.waitForSelector('#drawer.on');await sleep(400);
  ok('móvil: los botones de prueba miden 36 px o más y caben sin scroll horizontal',await q.evaluate(()=>[...document.querySelectorAll('#d_tone button')].every(b=>b.getBoundingClientRect().height>=35.5)&&document.documentElement.scrollWidth<=window.innerWidth+1));
  await q.evaluate(()=>document.getElementById('d_tone').scrollIntoView({block:'center'}));await sleep(200);
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_cajon_probar.png'});
  await q.tap('#dclose');
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
  // ── Temas: Oscuro, Grafito, Claro (suave) y Automático ──
  const lum=h=>{const m=h.match(/\d+/g).map(Number),f=v=>{v/=255;return v<=.03928?v/12.92:Math.pow((v+.055)/1.055,2.4);};return .2126*f(m[0])+.7152*f(m[1])+.0722*f(m[2]);};
  const cr=(a,b)=>{const x=lum(a),y=lum(b);return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);};
  const rgbOf=hex=>{const h=hex.replace('#','');return 'rgb('+[0,2,4].map(i=>parseInt(h.substr(i,2),16)).join(',')+')';};
  const colors=()=>p.evaluate(()=>{const g=(el,pr)=>getComputedStyle(el)[pr],v=k=>getComputedStyle(document.documentElement).getPropertyValue(k).trim();return {bg:g(document.body,'backgroundColor'),fg:g(document.body,'color'),
    mut:g(document.querySelector('.hint'),'color'),panel:v('--panel'),ac:v('--ac'),ok:v('--ok'),bad:v('--bad'),warn:v('--warn'),onac:v('--on-ac'),
    theme:document.documentElement.dataset.theme,meta:document.querySelector('meta[name=theme-color]').content,cs:getComputedStyle(document.documentElement).colorScheme};});
  const setTheme=async t=>{await p.evaluate(t=>localStorage.setItem('sonos_theme',t),t);await p.reload();await p.waitForSelector('.spk');return colors();};
  await p.evaluate(()=>localStorage.removeItem('sonos_theme'));await p.reload();await p.waitForSelector('.spk');
  let c=await colors();
  ok('Tema: por defecto oscuro (fondo negro, theme-color #000000)',c.theme==='dark'&&c.bg==='rgb(0, 0, 0)'&&c.meta==='#000000'&&c.cs==='dark',c);
  ok('Tema: el botón existe y dice Oscuro',(await p.textContent('[data-theme-btn]')).includes('Oscuro'));
  const names=[];for(let i=0;i<4;i++){await p.click('[data-theme-btn]');names.push((await p.textContent('[data-theme-btn]')).trim());}
  ok('Tema: el botón rota Grafito → Claro → Automático → Oscuro',JSON.stringify(names)==='["Grafito","Claro","Automático","Oscuro"]',names);
  const noPurple=x=>['ac','bad','ok','warn'].every(k=>{const h=x[k].replace('#','');const r=parseInt(h.substr(0,2),16),g=parseInt(h.substr(2,2),16),b=parseInt(h.substr(4,2),16);return !(b>r&&r>g&&b>150&&r>100);});
  for(const [t,meta,cs] of [['graphite','#1c1c1e','dark'],['light','#d5d9e0','light']]){
    c=await setTheme(t);
    ok('Tema '+t+': data-theme, theme-color y color-scheme correctos',c.theme===t&&c.meta===meta&&c.cs===cs,c);
    ok('Tema '+t+': texto/fondo y texto atenuado ≥ 4,5:1',cr(c.fg,c.bg)>=4.5&&cr(c.mut,c.bg)>=4.5,[cr(c.fg,c.bg),cr(c.mut,c.bg)]);
    ok('Tema '+t+': acento y estados (ok, aviso, error) ≥ 4,5:1 sobre el fondo',['ac','ok','warn','bad'].every(k=>cr(rgbOf(c[k]),c.bg)>=4.5),['ac','ok','warn','bad'].map(k=>[k,cr(rgbOf(c[k]),c.bg)]));
    ok('Tema '+t+': texto del botón primario sobre el acento ≥ 4,5:1',cr(rgbOf(c.onac),rgbOf(c.ac))>=4.5,[c.onac,c.ac]);
    ok('Tema '+t+': sin morado en los acentos',noPurple(c),c);
    ok('Tema '+t+': persiste al recargar (sin destello)',await p.evaluate(()=>localStorage.getItem('sonos_theme'))===t&&c.theme===t);
    if(SHOTS)await p.screenshot({path:SHOTS+'/tema_'+t+'_panel.png',fullPage:false});
  }
  c=await setTheme('light');
  ok('Tema claro suave: ni el fondo ni las tarjetas son blanco (luminancia < 0,72 y < 0,80)',lum(c.bg)<0.72&&lum(rgbOf(c.panel))<0.80&&lum(c.bg)>0.4,[lum(c.bg),lum(rgbOf(c.panel))]);
  c=await setTheme('graphite');
  ok('Tema Grafito: gris, no negro puro (fondo entre 0,008 y 0,05 de luminancia)',lum(c.bg)>0.008&&lum(c.bg)<0.05,lum(c.bg));
  await p.emulateMedia({colorScheme:'dark'});c=await setTheme('auto');
  ok('Tema Automático: sigue al sistema (aquí oscuro)',(await p.textContent('[data-theme-btn]')).includes('Automático')&&c.theme==='dark',c);
  await p.emulateMedia({colorScheme:'light'});await sleep(200);c=await colors();
  ok('Tema Automático: cambia a claro cuando el sistema pasa a claro',c.theme==='light',c);
  await p.emulateMedia({colorScheme:'dark'});await sleep(200);c=await colors();
  ok('Tema Automático: vuelve a oscuro',c.theme==='dark',c);
  for(const u of ['/calibrate','/ble']){await p.evaluate(()=>localStorage.setItem('sonos_theme','graphite'));const q=await ctx.newPage();await q.goto(BASE+u);ok('Tema Grafito compartido en '+u,await q.evaluate(()=>document.documentElement.dataset.theme)==='graphite'&&await q.locator('[data-theme-btn]').count()===1);await q.close();}
  // botones de cabecera: todos con la misma altura y alineados (antes «Calibrar» era un enlace desplazado)
  await p.evaluate(()=>localStorage.removeItem('sonos_theme'));await p.reload();await p.waitForSelector('.spk');
  const hb=await p.evaluate(()=>['[data-theme-btn]','#lcal','#lble'].map(s=>{const r=document.querySelector(s).getBoundingClientRect();return [r.top,r.height];}));
  ok('Cabecera: Calibrar, Añadir altavoces y Tema son botones alineados (misma altura y borde superior)',hb.every(x=>Math.abs(x[0]-hb[0][0])<1&&Math.abs(x[1]-hb[0][1])<1),hb);
  ok('Cabecera: «Calibrar» no se subraya como enlace',await p.evaluate(()=>getComputedStyle(document.getElementById('lcal')).textDecorationLine)==='none');

  // ── Plano de la sala: un dibujo por altavoz (canal), con ESP, canal y estado; se mueven uno a uno ──
  const spkPos=async(id,side)=>p.evaluate(([i,sd])=>{const e=document.querySelector('#spks .spk[data-id="'+i+'"][data-side="'+sd+'"]'),r=document.getElementById('room').getBoundingClientRect(),b=e.getBoundingClientRect();return [(b.left+b.width/2-r.left)/r.width*100,(b.top+b.height/2-r.top)/r.height*100];},[id,side]);
  const U=(id,side)=>'#spks .spk[data-id="'+id+'"][data-side="'+side+'"]';
  ok('Plano: pantalla, «Atrás» y «Escucha»',(await p.textContent('.screen'))==='Pantalla'&&(await p.textContent('.rl.back'))==='Atrás'&&(await p.textContent('.me small'))==='Escucha');
  ok('Plano: 5 ESP estéreo = 10 altavoces dibujados (uno por canal)',await p.locator('#spks .spk').count()===10&&await p.locator('#spks .spk .cab svg').count()===10);
  const lab=await p.evaluate(()=>[...document.querySelectorAll('#spks .spk')].map(e=>[e.dataset.id,e.dataset.side,e.querySelector('.ch').textContent,e.querySelector('.cap').textContent]));
  ok('Plano: cada altavoz muestra su canal, el ESP y el estado (ESP 0: FL y FR, «En línea»)',lab.some(x=>x[0]==='0'&&x[1]==='0'&&x[2]==='FL'&&x[3]==='ESP 0 · En línea')&&lab.some(x=>x[0]==='0'&&x[1]==='1'&&x[2]==='FR'&&x[3]==='ESP 0 · En línea'),lab);
  ok('Plano: sin líneas entre altavoces ni canales fantasma',await p.locator('#spks line,#spks ellipse,#room .gd,#room .ln').count()===0);
  // Fondo de radar: anillos cada 0,5 m centrados en «Escucha» (el ancho del plano son 5 m)
  const rd=await p.evaluate(()=>{const r=document.getElementById('room').getBoundingClientRect(),m=document.querySelector('.me').getBoundingClientRect(),cs=[...document.querySelectorAll('#radar circle')],
      rad=cs.map(c=>+c.getAttribute('r')),cx=+cs[0].getAttribute('cx'),cy=+cs[0].getAttribute('cy');
    return {n:cs.length,step:rad[1]-rad[0],stepOK:rad.every((v,i)=>Math.abs(v-(i+1)*document.getElementById('room').clientWidth/10)<0.6),ex:Math.abs(cx-(m.left+m.width/2-r.left)),ey:Math.abs(cy-(m.top+m.height/2-r.top)),
      labels:[...document.querySelectorAll('#radar text')].map(x=>x.textContent),major:document.querySelectorAll('#radar .ring.m').length,mw:m.width,z:getComputedStyle(document.getElementById('radar')).zIndex,w:r.width};});
  ok('Plano radar: anillos cada 0,5 m (ancho/10 px) y centrados en «Escucha»',rd.n>=6&&rd.stepOK&&rd.ex<1.5&&rd.ey<1.5,rd);
  ok('Plano radar: marca cada metro («1 m», «2 m»…) con el anillo del metro más marcado',rd.labels[0]==='1 m'&&rd.labels[1]==='2 m'&&rd.major>=3,rd);
  ok('Plano radar: queda detrás de los altavoces y «Escucha» es más grande (≥ 60 px)',rd.z==='0'&&rd.mw>=60,rd);
  // Volumen y retardo de la calibración en cada altavoz
  const calTxt=async(id,sd)=>p.evaluate(([i,s])=>document.querySelector('#spks .spk[data-id="'+i+'"][data-side="'+s+'"] .cal').textContent.replace(/\s+/g,' ').trim(),[id,sd]);
  ok('Plano: cada altavoz muestra volumen y retardo con formato «NN% · N,N ms»',/^\d+% · −?\d+,\d ms$/.test(await calTxt(0,0))&&await p.locator('#spks .spk .cal').count()===10,await calTxt(0,0));
  await p.evaluate(async()=>{await fetch('/api/node',{method:'POST',body:JSON.stringify({id:0,volume_l:128,volume_r:64})});await fetch('/api/calibrate',{method:'POST',body:JSON.stringify({esp_id:0,latency_us:2500})});});
  ok('Plano: el volumen (por lado) y el retardo calibrados se actualizan solos (FL 50 %, FR 25 %, 2,5 ms)',await until(async()=>(await calTxt(0,0))==='50% · 2,5 ms'&&(await calTxt(0,1))==='25% · 2,5 ms'),await calTxt(0,0)+' | '+await calTxt(0,1));
  await p.evaluate(async()=>{await fetch('/api/node',{method:'POST',body:JSON.stringify({id:0,volume_l:255,volume_r:255})});await fetch('/api/calibrate',{method:'POST',body:JSON.stringify({esp_id:0,latency_us:0})});});
  ok('Plano: al restablecer vuelve a «100% · 0,0 ms»',await until(async()=>(await calTxt(0,0))==='100% · 0,0 ms'));
  ok('Plano: el estado no depende solo del color (punto + texto)',await p.locator('#spks .spk .cap .dot').count()===10&&lab.every(x=>/En línea|Sin conexión/.test(x[3])));
  ok('Plano: fuera del modo mover, tocar un altavoz abre el cajón de su ESP',await (async()=>{await p.click(U(0,1));const o=await p.evaluate(()=>document.getElementById('drawer').classList.contains('on'));await p.click('#drawer .x');return o;})());
  const before0=await spkPos(0,0),before1=await spkPos(0,1);const vol00=(await J('/api/nodes'))[0].volume_l;
  await p.click('#medit');
  ok('Plano: «Mover altavoces» activa el modo (aria-pressed, botón Listo, restablecer visible)',await p.getAttribute('#medit','aria-pressed')==='true'&&(await p.textContent('#medit'))==='Listo'&&await p.isVisible('#mreset'));
  await p.click(U(0,1),{force:true});
  ok('Plano: en modo mover, tocar no abre el cajón',!(await p.evaluate(()=>document.getElementById('drawer').classList.contains('on'))));
  const rb=await p.locator('#room').boundingBox();
  const drag1=async(sel,fx,fy)=>{const bb=await p.locator(sel+' .cab').boundingBox();const sx=bb.x+bb.width/2,sy=bb.y+bb.height/2,tx=rb.x+rb.width*fx,ty=rb.y+rb.height*fy;
    await p.mouse.move(sx,sy);await p.mouse.down();await p.mouse.move((sx+tx)/2,(sy+ty)/2,{steps:6});await p.mouse.move(tx,ty,{steps:6});await p.mouse.up();};
  await drag1(U(0,1),0.30,0.55);
  await until(async()=>(await J('/api/nodes'))[0].pos2_x>=0);
  let n0=(await J('/api/nodes'))[0];await sleep(300);const after1=await spkPos(0,1),after0=await spkPos(0,0);
  ok('Plano: arrastrar el altavoz FR guarda su posición (pos2 ≈ 30 %, 55 %)',Math.abs(n0.pos2_x-30)<2.5&&Math.abs(n0.pos2_y-55)<6,[n0.pos2_x,n0.pos2_y]);
  ok('Plano: el altavoz FL (mismo ESP) no se mueve y queda sin posición propia',n0.pos_x===-1&&Math.abs(after0[0]-before0[0])<1&&Math.abs(after0[1]-before0[1])<1,[n0.pos_x,before0,after0]);
  ok('Plano: FR queda donde se soltó',Math.abs(after1[0]-30)<2.5&&Math.abs(after1[1]-55)<6&&Math.abs(after1[0]-before1[0])>5,[before1,after1]);
  ok('Plano: arrastrar no cambia canales ni volumen',n0.ch_left===0&&n0.ch_right===1&&n0.volume_l===vol00&&n0.volume_r===255,[n0,vol00]);
  await drag1(U(0,0),0.70,0.30);
  await until(async()=>(await J('/api/nodes'))[0].pos_x>=0);
  n0=(await J('/api/nodes'))[0];
  ok('Plano: FL se mueve por separado y guarda su propia posición (≈ 70 %, 30 %); FR conserva la suya',Math.abs(n0.pos_x-70)<2.5&&Math.abs(n0.pos_y-30)<6&&Math.abs(n0.pos2_x-30)<2.5,[n0.pos_x,n0.pos_y,n0.pos2_x]);
  // límites: soltar fuera del plano se queda dentro
  await sleep(300);const bb1=await p.locator(U(0,0)+' .cab').boundingBox();
  await p.mouse.move(bb1.x+bb1.width/2,bb1.y+bb1.height/2);await p.mouse.down();await p.mouse.move(rb.x-300,rb.y-300,{steps:8});await p.mouse.up();
  await until(async()=>(await J('/api/nodes'))[0].pos_x<10);
  n0=(await J('/api/nodes'))[0];
  ok('Plano: arrastrar fuera del plano deja el altavoz entero dentro (no se corta por los bordes)',await (async()=>{await sleep(300);const q=await p.evaluate(()=>{const r=document.getElementById('room').getBoundingClientRect(),b=document.querySelector('#spks .spk[data-id="0"][data-side="0"]').getBoundingClientRect();return [b.left>=r.left-1,b.top>=r.top-1,b.right<=r.right+1,b.bottom<=r.bottom+1];});return q.every(Boolean)&&n0.pos_x>=3&&n0.pos_y>=8;})(),[n0.pos_x,n0.pos_y]);
  // teclado: ESP 1, canal derecho (SR... el altavoz 1 es C + LFE: lado 1 = LFE)
  await p.focus(U(1,1));await p.keyboard.press('ArrowRight');await p.keyboard.press('ArrowDown');await p.keyboard.press('Shift+ArrowDown');
  const k1=await until(async()=>(await J('/api/nodes'))[1].pos2_x>=0,3000);
  const n1=(await J('/api/nodes'))[1];const p1=await spkPos(1,1);
  ok('Plano: las flechas mueven el altavoz concreto (canal derecho del ESP 1), se guarda y el foco se mantiene',k1&&n1.pos_x===-1&&await p.evaluate(()=>[document.activeElement.dataset.id,document.activeElement.dataset.side].join())==='1,1'&&Math.abs(n1.pos2_x-p1[0])<1.5&&Math.abs(n1.pos2_y-p1[1])<1.5,[n1.pos2_x,n1.pos2_y,p1]);
  if(SHOTS)await p.screenshot({path:SHOTS+'/plano_mover.png',fullPage:true});
  await p.reload();await p.waitForSelector('.spk');
  const rl=await spkPos(0,0);ok('Plano: tras recargar el altavoz sigue en su sitio',Math.abs(rl[0]-n0.pos_x)<1.5&&Math.abs(rl[1]-n0.pos_y)<1.5,[rl,n0.pos_x,n0.pos_y]);
  // Zoom del plano: cambia la distancia entre cosas (y los anillos de metros), nunca el tamaño de los altavoces ni de «Escucha»
  const geo=async()=>p.evaluate(()=>{const R=document.getElementById('room').getBoundingClientRect(),c=e=>{const b=e.getBoundingClientRect();return [b.left+b.width/2-R.left,b.top+b.height/2-R.top,b.width,b.height];},cs=[...document.querySelectorAll('#radar circle')].map(x=>+x.getAttribute('r'));
    return {fr:c(document.querySelector('#spks .spk[data-id="0"][data-side="1"]')),me:c(document.querySelector('#room .me')),step:cs[1]-cs[0],z:document.getElementById('zval').textContent,W:document.getElementById('room').clientWidth,H:document.getElementById('room').clientHeight,
      zin:document.getElementById('zin').disabled,zout:document.getElementById('zout').disabled,zoomed:document.getElementById('room').classList.contains('zoomed')};});
  const vec=g=>[g.fr[0]-g.me[0],g.fr[1]-g.me[1]];
  const same=(a,b)=>Math.abs(a.fr[2]-b.fr[2])<0.6&&Math.abs(a.fr[3]-b.fr[3])<0.6&&Math.abs(a.me[2]-b.me[2])<0.6&&Math.abs(a.me[3]-b.me[3])<0.6;
  const vuIn=async()=>p.evaluate(()=>[...document.querySelectorAll('#spks .spk')].every(b=>{const r=b.getBoundingClientRect(),v=b.querySelector('.vu').getBoundingClientRect(),c=b.querySelector('.cab').getBoundingClientRect();return v.left>=r.left-0.5&&v.right<=r.right+0.5&&v.top>=r.top-0.5&&v.bottom<=r.bottom+0.5&&c.left>=r.left-0.5&&c.right<=r.right+0.5;}));
  ok('zoom 100 %: el vúmetro y el dibujo quedan dentro del recuadro de cada altavoz',await vuIn());
  const g0=await geo();
  const zsz=await p.evaluate(()=>['zin','zout','zval'].map(i=>{const b=document.getElementById(i).getBoundingClientRect();return [b.width,b.height];}));
  ok('zoom: controles − / % / + con objetivos ≥ 44 px y marca «100%»',g0.z==='100%'&&zsz.every(x=>x[0]>=44&&x[1]>=44)&&!g0.zoomed,[g0.z,zsz]);
  await p.click('#zin');await sleep(120);const g1=await geo();
  ok('zoom: «+» acerca a 125 %: FR se aleja de «Escucha» ×1,25 y los anillos también',g1.z==='125%'&&Math.abs(vec(g1)[0]/vec(g0)[0]-1.25)<0.03&&Math.abs(vec(g1)[1]/vec(g0)[1]-1.25)<0.03&&Math.abs(g1.step/g0.step-1.25)<0.02,[vec(g0),vec(g1),g0.step,g1.step]);
  ok('zoom: los altavoces y «Escucha» conservan su tamaño y «Escucha» no se mueve',same(g0,g1)&&Math.abs(g1.me[0]-g0.me[0])<0.6&&Math.abs(g1.me[1]-g0.me[1])<0.6,[g0.fr,g1.fr,g0.me,g1.me]);
  for(let i=0;i<6;i++)if(!(await geo()).zin)await p.click('#zin');
  const g3=await geo();
  ok('zoom 300 %: el vúmetro queda dentro del recuadro del altavoz',await vuIn());
  ok('zoom: máximo 300 % (el botón + se desactiva) y sigue sin cambiar el tamaño de los altavoces',g3.z==='300%'&&g3.zin&&g3.zoomed&&same(g0,g3),[g3.z,g3.zin]);
  const bgPt=async()=>p.evaluate(()=>{const R=document.getElementById('room').getBoundingClientRect();for(let fy=0.55;fy<0.95;fy+=0.05)for(let fx=0.1;fx<0.95;fx+=0.05){const x=R.left+R.width*fx,y=R.top+R.height*fy;if(document.elementFromPoint(x,y)===document.getElementById('room'))return [x,y];}return null;});
  const bp0=await bgPt();await p.mouse.move(bp0[0],bp0[1]);await p.mouse.down();await p.mouse.move(bp0[0]-80,bp0[1]-60,{steps:6});await p.mouse.up();await sleep(100);const g4=await geo();
  ok('zoom: con zoom, arrastrar el fondo mueve el plano (≈ −80, −60 px) sin mover «Escucha» de sitio relativo a los anillos',Math.abs(g4.fr[0]-g3.fr[0]+80)<2&&Math.abs(g4.fr[1]-g3.fr[1]+60)<2&&Math.abs(g4.me[0]-g3.me[0]+80)<2,[g3.fr,g4.fr]);
  for(let i=0;i<12;i++)if(!(await geo()).zout)await p.click('#zout');
  const gm=await geo();
  ok('zoom: mínimo 50 % (el botón − se desactiva): «Escucha» no se mueve, el plano se encoge y los altavoces se achican (≈ 55 %) para despejar',gm.z==='50%'&&gm.zout&&Math.abs(gm.me[0]-g0.me[0])<1&&Math.abs(gm.me[1]-g0.me[1])<1&&Math.hypot(...vec(gm))<Math.hypot(...vec(g0))&&gm.fr[2]<g0.fr[2]*0.6&&gm.me[2]<g0.me[2]*0.6,[gm.z,gm.fr[2],g0.fr[2]]);
  ok('zoom: alejado se ocultan el estado y el volumen/retardo (queda canal y vúmetro); a 100 % o más están visibles',await p.evaluate(()=>[...document.querySelectorAll('#spks .spk .cap,#spks .spk .cal')].every(e=>getComputedStyle(e).display==='none'&&true))&&await p.evaluate(()=>getComputedStyle(document.querySelector('#spks .spk .ch')).display!=='none'&&getComputedStyle(document.querySelector('#spks .spk .vu')).display!=='none'));
  ok('zoom 50 % (sin etiquetas): el vúmetro queda dentro del recuadro del altavoz',await vuIn());
  await p.click('#zval');await sleep(100);const gr=await geo();
  ok('zoom: pulsar el porcentaje vuelve a 100 % y todo al sitio de antes',gr.z==='100%'&&!gr.zoomed&&Math.abs(gr.fr[0]-g0.fr[0])<1&&Math.abs(gr.fr[1]-g0.fr[1])<1&&Math.abs(gr.step-g0.step)<0.5,[gr.fr,g0.fr]);
  await p.evaluate(()=>{const r=document.getElementById('room'),b=r.getBoundingClientRect();r.dispatchEvent(new WheelEvent('wheel',{deltaY:-60,ctrlKey:true,clientX:b.left+300,clientY:b.top+300,bubbles:true,cancelable:true}));});
  await sleep(100);const gw=await geo();
  ok('zoom: Ctrl + rueda (o pellizco del trackpad) acerca el plano',parseInt(gw.z)>150,gw.z);
  await p.click('#zval');
  await p.evaluate(()=>{const r=document.getElementById('room'),b=r.getBoundingClientRect(),mk=(t,id,x,y)=>r.dispatchEvent(new PointerEvent(t,{pointerId:id,pointerType:'touch',clientX:b.left+x,clientY:b.top+y,bubbles:true,isPrimary:id===11}));
    mk('pointerdown',11,500,450);mk('pointerdown',12,600,450);for(let i=1;i<=5;i++){mk('pointermove',11,500-i*20,450);mk('pointermove',12,600+i*20,450);}mk('pointerup',11,400,450);mk('pointerup',12,700,450);});
  await sleep(100);const gp=await geo();
  ok('zoom: el pellizco con dos dedos acerca el plano (abrir los dedos ×3)',parseInt(gp.z)>=250&&same(g0,gp),gp.z);
  await p.click('#zval');
  // Con zoom también se mueven los altavoces: 1 px en pantalla = 1/(ancho × zoom) de la sala
  await p.evaluate(()=>setZoom(2));await p.click('#medit');await sleep(150);
  const nz0=(await J('/api/nodes'))[0],gz=await geo(),cabz=await p.locator(U(0,1)+' .cab').boundingBox();
  await p.mouse.move(cabz.x+cabz.width/2,cabz.y+cabz.height/2);await p.mouse.down();await p.mouse.move(cabz.x+cabz.width/2+60,cabz.y+cabz.height/2,{steps:6});await p.mouse.up();
  await until(async()=>(await J('/api/nodes'))[0].pos2_x!==nz0.pos2_x);const nz1=(await J('/api/nodes'))[0];
  ok('zoom: al arrastrar un altavoz con zoom 200 %, 60 px equivalen a 60/(2·ancho) de la sala',Math.abs((nz1.pos2_x-nz0.pos2_x)-60/(gz.W*2)*100)<0.6&&Math.abs(nz1.pos2_y-nz0.pos2_y)<0.6,[nz0.pos2_x,nz1.pos2_x,60/(gz.W*2)*100]);
  await p.click('#medit');await p.evaluate(()=>setZoom(1));await p.click('#zval');await sleep(100);
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:0,pos2_x:nz0.pos2_x,pos2_y:nz0.pos2_y})});await p.reload();await p.waitForSelector('.spk');
  await p.click('#medit');await p.click('#mreset');
  await until(async()=>(await J('/api/nodes')).every(n=>n.pos_x<0&&n.pos2_x<0));
  ok('Plano: «Restablecer posiciones» devuelve ambos canales de todos al lugar de su canal',(await J('/api/nodes')).every(n=>n.pos_x<0&&n.pos_y<0&&n.pos2_x<0&&n.pos2_y<0));
  await p.click('#medit');
  ok('Plano: «Listo» sale del modo mover',await p.getAttribute('#medit','aria-pressed')==='false'&&!(await p.isVisible('#mreset')));
  // móvil: arrastre táctil, temas sin desbordes y botones alineados
  const ctx2=await b.newContext({viewport:{width:390,height:844},deviceScaleFactor:2,isMobile:true,hasTouch:true});
  const mp=await ctx2.newPage();await mp.goto(BASE+'/');await mp.waitForSelector('.spk');
  for(const t of ['graphite','light']){await mp.evaluate(t=>localStorage.setItem('sonos_theme',t),t);await mp.reload();await mp.waitForSelector('.spk');
    ok('Móvil: tema '+t+' sin desbordamiento horizontal',await mp.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));}
  const mh=await mp.evaluate(()=>['[data-theme-btn]','#lcal','#lble'].map(s=>{const r=document.querySelector(s).getBoundingClientRect();return [r.top,r.height];}));
  ok('Móvil: los botones de la cabecera están alineados',mh.every(x=>Math.abs(x[0]-mh[0][0])<1&&Math.abs(x[1]-mh[0][1])<1),mh);
  ok('Móvil: los altavoces con posición automática no se solapan entre sí',await mp.evaluate(()=>{const bs=[...document.querySelectorAll('#spks .spk')].map(e=>e.getBoundingClientRect());let o=0;for(let i=0;i<bs.length;i++)for(let j=i+1;j<bs.length;j++){const a=bs[i],b=bs[j];if(a.left<b.right-4&&b.left<a.right-4&&a.top<b.bottom-4&&b.top<a.bottom-4)o++;}return o===0;}));
  if(SHOTS){await mp.evaluate(()=>window.scrollTo(0,document.getElementById('room').getBoundingClientRect().top+scrollY-70));await mp.screenshot({path:SHOTS+'/movil_plano_auto.png'});}
  await mp.click('#medit');
  await mp.evaluate(()=>window.scrollTo(0,document.getElementById('room').getBoundingClientRect().top+scrollY-70));await sleep(200);const sy0=await mp.evaluate(()=>scrollY);
  const mrb=await mp.locator('#room').boundingBox(),mb0=await mp.locator(U(2,0)+' .cab').boundingBox();
  const cdp=await ctx2.newCDPSession(mp);const sx2=mb0.x+mb0.width/2,sy2=mb0.y+mb0.height/2,tx2=mrb.x+mrb.width*0.7,ty2=mrb.y+mrb.height*0.45;
  const tp=(t,x,y)=>cdp.send('Input.dispatchTouchEvent',{type:t,touchPoints:t==='touchEnd'?[]:[{x,y,id:1}]});
  await tp('touchStart',sx2,sy2);for(let i=1;i<=8;i++)await tp('touchMove',sx2+(tx2-sx2)*i/8,sy2+(ty2-sy2)*i/8);await tp('touchEnd');
  const okT=await until(async()=>(await J('/api/nodes'))[2].pos_x>=0,3000);const m2=(await J('/api/nodes'))[2];
  ok('Móvil: arrastre táctil mueve el altavoz (≈70 %, 45 %) sin desplazar la página',okT&&Math.abs(m2.pos_x-70)<4&&Math.abs(m2.pos_y-45)<7&&await mp.evaluate(()=>scrollY)===sy0,[m2.pos_x,m2.pos_y,sy0]);
  ok('Móvil: ningún altavoz se corta por los bordes del plano ni tapa la pantalla',await mp.evaluate(()=>{const r=document.getElementById('room').getBoundingClientRect(),sc=document.querySelector('.screen').getBoundingClientRect();return [...document.querySelectorAll('#spks .spk')].every(e=>{const b=e.getBoundingClientRect();return b.left>=r.left-1&&b.right<=r.right+1&&b.bottom<=r.bottom+1&&b.top>=sc.bottom-1;});}));
  if(SHOTS)await mp.screenshot({path:SHOTS+'/movil_claro_plano.png'});
  // Zoom en el móvil: más espacio entre altavoces sin agrandarlos
  const mg=async()=>mp.evaluate(()=>{const R=document.getElementById('room').getBoundingClientRect(),bs=[...document.querySelectorAll('#spks .spk')].map(e=>e.getBoundingClientRect());
    let ov=0;for(let i=0;i<bs.length;i++)for(let j=i+1;j<bs.length;j++){const a=bs[i],b=bs[j];if(a.left<b.right-2&&b.left<a.right-2&&a.top<b.bottom-2&&b.top<a.bottom-2)ov++;}
    const m=document.querySelector('#room .me').getBoundingClientRect();
    return {cap:getComputedStyle(document.querySelector('#spks .spk .cap')).display,w:bs[0].width,h:bs[0].height,mw:m.width,ov,z:document.getElementById('zval').textContent,ta:getComputedStyle(document.getElementById('room')).touchAction};});
  const zm0=await mg();await mp.locator('#room').scrollIntoViewIfNeeded();await sleep(600);   // sin esta pausa el primer toque puede caer mientras la página aún se desplaza
  await mp.tap('#zin');await sleep(250);await mp.tap('#zin');await sleep(250);const zm1=await mg();
  ok('Móvil: tocar «+» dos veces acerca a 156 % sin cambiar el tamaño de altavoces ni de «Escucha»; el fondo deja de deslizar la página',zm1.z==='156%'&&Math.abs(zm1.w-zm0.w)<0.6&&Math.abs(zm1.h-zm0.h)<0.6&&zm1.mw===zm0.mw&&zm1.ta==='none'&&zm0.ta==='pan-y',[zm0,zm1]);
  if(SHOTS){await mp.screenshot({path:SHOTS+'/movil_zoom_mas.png'});}
  await mp.tap('#zval');await mp.tap('#zout');await mp.tap('#zout');await sleep(150);const zm2=await mg();
  ok('Móvil: alejar a 64 % achica los altavoces (≈ 64 %), oculta las etiquetas y se solapan bastantes menos (5 → ≤ 2)',zm2.z==='64%'&&zm2.w<zm0.w*0.75&&zm2.ov<zm0.ov&&zm2.ov<=2&&zm2.cap==='none',[zm0,zm2]);
  ok('Móvil 64 %: el vúmetro queda dentro del recuadro del altavoz',await mp.evaluate(()=>[...document.querySelectorAll('#spks .spk')].every(b=>{const r=b.getBoundingClientRect(),v=b.querySelector('.vu').getBoundingClientRect();return v.left>=r.left-0.5&&v.right<=r.right+0.5;})));
  if(SHOTS){await mp.screenshot({path:SHOTS+'/movil_zoom_menos.png'});}
  await mp.tap('#zval');await sleep(100);
  ok('Móvil: el control de zoom no tapa el plano ni se sale de la pantalla (≥ 44 px)',await mp.evaluate(()=>{const z=document.querySelector('.zoomctl').getBoundingClientRect(),r=document.getElementById('room').getBoundingClientRect();return z.right<=r.right&&z.top>=r.top&&z.height>=44&&innerWidth>=z.right;}));
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
