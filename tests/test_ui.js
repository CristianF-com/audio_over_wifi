// Pruebas de interfaz con Playwright (Chromium): panel, /ble (Web Bluetooth simulado) y /calibrate (resultados inyectados).
// Requiere el arnés en :18080 y feed_nodes.py N. Uso: NODE_PATH=$(npm root -g) node test_ui.js [carpeta_capturas]
const {chromium}=require('playwright');
const BASE='http://127.0.0.1:18080', SHOTS=process.argv[2]||null;
let fails=0;const ok=(n,c,x)=>{console.log((c?'PASS ':'FAIL ')+'UI '+n+(c?'':'  '+(x===undefined?'':JSON.stringify(x))));if(!c)fails++;};
const J=async(p)=>(await fetch(BASE+p)).json();
const adv=pg=>pg.evaluate(()=>{const a=document.getElementById('d_adv');if(a)a.open=true;});   // los ajustes avanzados del altavoz vienen plegados
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function until(f,ms=4000){const t=Date.now();while(Date.now()-t<ms){try{if(await f())return true;}catch(e){}await sleep(100);}return false;}
(async()=>{
  const b=await chromium.launch();
  const ctx=await b.newContext({viewport:{width:1280,height:860}});
  await ctx.addInitScript(()=>{try{if(localStorage.getItem('sonos_shape')===null)localStorage.setItem('sonos_shape','round');}catch(e){}});
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
  // Fila superior: un cuadrado por canal (vacío sin señal, verde = cable conectado, rojo = destello por nivel) con el nivel debajo
  const lamps=await p.evaluate(()=>[...document.querySelectorAll('#mb .mc')].map(m=>({on:m.querySelector('.lamp').classList.contains('on'),num:m.querySelector('.lvn').textContent})));
  ok('vúmetros: 10 cuadrados indicadores, uno por canal',lamps.length===10);
  const zones=await p.evaluate(()=>[...document.querySelectorAll('#mb .lamp')].map(l=>l.dataset.st));
  ok('vúmetros: FL, FR, C y canal 5 (≥ −40 dBFS) en zona «con sonido»; el resto sin señal',zones.join()==='sig,sig,sig,off,sig,off,off,off,off,off',zones);
  ok('vúmetros: el número bajo cada cuadrado es el nivel (−6, −6, −12, —, −12)',lamps.slice(0,5).map(l=>l.num).join('|')==='−6|−6|−12|—|−12',lamps);
  const lp=await p.evaluate(()=>{const g=id=>getComputedStyle(document.getElementById(id)),on=g('lp0'),off=g('lp3');
    document.getElementById('lp3').dataset.st='cable';const cab=g('lp3').backgroundColor,cabAnim=g('lp3').animationName;document.getElementById('lp3').dataset.st='off';
    document.getElementById('lp0').classList.add('on');const red=g('lp0').backgroundColor,sh=g('lp0').boxShadow,fl=g('lp0').filter;document.getElementById('lp0').classList.remove('on');
    return {sh,fl,red,offBg:off.backgroundColor,cab,anim:on.animationName,cabAnim,animOff:off.animationName,w:off.width,h:off.height};});
  ok('vúmetros: sin señal = vacío; cable conectado = verde fijo; destello = rojo liso, sin brillo ni sombra; ninguno usa animación continua',lp.red==='rgb(255, 90, 80)'&&lp.cab==='rgb(52, 199, 89)'&&lp.offBg!==lp.cab&&lp.offBg!==lp.red&&lp.sh==='none'&&lp.fl==='none'&&lp.anim==='none'&&lp.cabAnim==='none'&&lp.animOff==='none'&&lp.w===lp.h,lp);
  // Lógica de umbrales: −70..−40 cable fijo; −40..0 destella al subir ~3 dB sobre el último valle; al bajar se apaga y vuelve a destellar
  const seq=await p.evaluate(()=>{const s={ref:-90,until:0,tm:0},r=[];const st=(db,t)=>{const z=lampStep(s,db,t);r.push([db,z,t<s.until]);};
    [-95,-60,-45,-39,-39,-37,-36,-30,-33,-36,-31,-90,-39,-60].forEach((db,i)=>st(db,i*1000));return r;});
  const sq=Object.fromEntries(seq.map(([db,z,f],i)=>[i,[z,f]]));
  ok('LED: por debajo de −70 apagado; de −70 a −40 «cable» sin destello',sq[0][0]==='off'&&!sq[0][1]&&sq[1][0]==='cable'&&!sq[1][1]&&sq[2][0]==='cable'&&!sq[2][1],seq);
  ok('LED: al entrar en −40 destella; mantenerse o subir menos de 3 dB no destella',sq[3][1]===true&&sq[4][1]===false&&sq[5][1]===false&&sq[6][1]===true,seq);   // −39 (entra), −39, −37 (+2), −36 (+3 sobre −39)
  ok('LED: sube 3 dB destella; baja no destella; vuelve a subir 3 dB desde el valle y destella',sq[7][1]===true&&sq[8][1]===false&&sq[9][1]===false&&sq[10][1]===true,seq);   // −30 (+6), −33, −36 (valle), −31 (+5)
  ok('LED: si cae a «cable» o a silencio se reinicia y al volver a −40 destella otra vez',sq[11][0]==='off'&&sq[12][1]===true&&sq[13][0]==='cable'&&!sq[13][1],seq);
  const fl=await p.evaluate(async()=>{const l=document.getElementById('lp1');LAMPS[1].ref=-90;LAMPS[1].until=0;const t=performance.now();const z=lampStep(LAMPS[1],-10,t);return z+':'+(t<LAMPS[1].until)+':'+LAMP_FLASH;});
  ok('LED: el destello dura un instante (140 ms), no es una animación continua',fl==='sig:true:140',fl);
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
  ok('plano: colores estándar verde/amarillo/rojo y planos (sin degradado)',cs.col.join('|')==='rgb(52, 199, 89)|rgb(255, 214, 10)|rgb(255, 90, 80)'&&cs.bg==='none',cs);
  ok('vúmetros: texto de pico ≈ −6 dBFS',(await p.textContent('#mtxt')).includes('−')===false&&(await p.textContent('#mtxt')).includes('-6'),await p.textContent('#mtxt'));
  const fake=async(o)=>{await p.route('**/api/levels',r=>r.fulfill({contentType:'application/json',body:JSON.stringify(Object.assign({capturing:true,signal:true,signal_age_ms:10,pps:900,sending:true,nodes_online:5,db:[-6,-6,-12,-90,-12,-90,-90,-90,-90,-90]},o))}));};
  await fake({capturing:false,signal:false,sending:false,db:[-90,-90,-90,-90,-90,-90,-90,-90,-90,-90]});
  ok('estado: sin captura -> «Sin captura de audio» (rojo) y todo apagado',await until(async()=>(await p.textContent('#hs'))==='Sin captura de audio'&&(await p.getAttribute('#dot','class')).includes('bad')&&(await p.getAttribute('#meters','class')).includes('off')&&(await p.locator('#mb .lamp.on').count())===0&&(await p.locator('#spks .vu .led.on').count())===0));
  await fake({signal:false,sending:false,db:[-90,-90,-90,-90,-90,-90,-90,-90,-90,-90]});
  ok('estado: silencio -> «Entrada conectada, sin señal» (ámbar)',await until(async()=>(await p.textContent('#hs'))==='Entrada conectada, sin señal'&&(await p.getAttribute('#dot','class')).includes('warn')&&(await p.textContent('#mtxt'))==='Silencio'));
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
  ok('cajón: nombre arriba y «Ajustes avanzados» plegados por defecto (rol, EQ, retardo dentro)',await p.evaluate(()=>!document.getElementById('d_adv').open&&!!document.getElementById('d_name')&&document.getElementById('d_adv').contains(document.getElementById('d_rl'))&&document.getElementById('d_adv').contains(document.getElementById('d_lat'))));
  await adv(p);
  ok('cajón abierto con título «Altavoz #1»',(await p.textContent('#dtitle'))==='Altavoz #1');
  await p.fill('#d_vl','128');await p.dispatchEvent('#d_vl','input');
  await p.click('#d_md button[data-v="1"]');
  await p.click('#d_rl button[data-v="2"]');
  await p.fill('#d_eq2','4');await p.dispatchEvent('#d_eq2','input');
  ok('autoguardado: volumen, mono, rol sub y EQ llegan al servidor',await until(async()=>{const x=(await J('/api/nodes'))[1];return x.volume_l===128&&x.mono===true&&x.role===2&&x.volume_r===128&&x.ch_right===x.ch_left&&Math.abs(x.eq[2]-4)<0.01;}),(await J('/api/nodes'))[1]);
  ok('mono oculta el canal derecho',!(await p.isVisible('#d_crw')));
  await p.fill('#d_name','Sofá');await p.dispatchEvent('#d_name','input');
  ok('nombre del altavoz: se guarda y sustituye a «ESP n» en el título y los chips',await until(async()=>(await J('/api/nodes'))[1].name==='Sofá'&&(await p.textContent('#dtitle')).includes('Sofá')||(await J('/api/nodes'))[1].name==='Sofá')&&await until(async()=>(await p.locator('#chips').textContent()).includes('Sofá')));
  await p.fill('#d_name','');await p.dispatchEvent('#d_name','input');
  ok('nombre vacío: vuelve a «ESP n»',await until(async()=>(await J('/api/nodes'))[1].name===''&&!(await p.locator('#chips').textContent()).includes('Sofá')));
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
  ok('Sonido abre en «Perfiles» (lo más usado primero)',(await p.getAttribute('#sb_pf','aria-selected'))==='true');
  await p.click('#sb_mode');
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
  await p.click('#t_proc');await p.waitForSelector('#subsel',{state:'visible'});
  await p.selectOption('#subsel','4');
  ok('elegir subwoofer asigna roles (4=sub, resto satélite)',await until(async()=>{const a=await J('/api/nodes');return a[4].role===2&&a[0].role===1;}));
  await p.selectOption('#subsel','-');
  ok('«Ninguno» devuelve todos a rango completo',await until(async()=>(await J('/api/nodes')).every(x=>x.role===0)));
  await p.click('#t_pass');await until(async()=>{const x=await J('/api/status');return !x.dsp_xover&&!x.dsp_eq;});
  // Latencia
  await p.click('.tab[data-tab="lat"]');await p.click('#plseg button[data-pl="120"]');
  ok('colchón de red 120 ms',await until(async()=>(await J('/api/status')).playout_ms===120));
  ok('tabla de red con 5 filas',await until(async()=>(await p.locator('#health tr').count())===5));
  ok('estado por altavoz: una tarjeta con veredicto (Bien/Inestable/Con cortes/Sin conexión) por cada altavoz conocido',await p.evaluate(()=>{const c=[...document.querySelectorAll('#hverd .vcard')];return c.length===document.querySelectorAll('#health tr').length&&c.every(x=>/v-(ok|warn|bad)/.test(x.className)&&/(Bien|Inestable|Con cortes|Sin conexión|Apagado)/.test(x.textContent));}));
  ok('la tabla de números va plegada («Ver todos los números»)',await p.evaluate(()=>!document.getElementById('hdet').open));
  ok('primeros pasos: la guía aparece con 4 pasos y «Ocultar guía» la oculta y lo recuerda',await (async()=>{await p.click('#tab_spk');const n=await p.locator('#wiz li').count();const vis=await p.isVisible('#wiz');await p.click('#wz_hide');const r=n===4&&vis&&await p.evaluate(()=>document.getElementById('wiz').hidden&&localStorage.getItem('sonos_wiz')==='off');await p.evaluate(()=>{localStorage.removeItem('sonos_wiz');drawWizard();});await p.click('.tab[data-tab="lat"]');return r;})());
  ok('comando mpv visible',(await p.textContent('#mpvcmd')).includes('--audio-delay='));
  await p.click('#plseg button[data-pl="80"]');
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_latencia.png',fullPage:true});
  // Añadir altavoces por Bluetooth desde la Raspberry (ayudante simulado)
  const fs=require('fs');await p.click('.tab[data-tab="spk"]');
  await p.evaluate(()=>{window._wmWait=1;});
  await p.click('#lble');await p.waitForSelector('#bdrawer.on');await p.waitForSelector('#b_scan');
  ok('BLE: el panel ya no trae el formulario de WiFi: la red y la clave se piden al pulsar «Configurar»',await p.evaluate(()=>!document.getElementById('b_ssid')&&!document.getElementById('b_pass')&&!document.getElementById('b_save')&&!document.getElementById('b_wscan')));
  await p.click('#b_scan');
  ok('BLE: lista 5 dispositivos',await until(async()=>(await p.locator('#bbody .found').count())===5,8000),await p.locator('#bbody .found').count());
  ok('BLE: 3 nuevos y ya no hay selector de número (lo asigna la Raspberry por MAC)',await p.evaluate(()=>[...document.querySelectorAll('.found')].filter(f=>f.textContent.includes('Nuevo')).length===3&&document.querySelectorAll('.found select').length===0&&!document.getElementById('bbody').textContent.includes('Número')));
  ok('BLE: el que no se pudo leer muestra el error',(await p.textContent('#bbody')).includes('No se pudo conectar: timeout'));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_bluetooth.png'});
  fs.writeFileSync('/tmp/sonos_ble_mock.log','');
  await p.click('.found[data-i="0"] [data-id]');
  ok('BLE: «Sonar» llega al ayudante',await until(async()=>fs.readFileSync('/tmp/sonos_ble_mock.log','utf8').includes('"identify"')));
  await p.click('.found[data-i="0"] [data-pv]');await p.waitForSelector('#wm:not([hidden])');
  ok('Configurar: abre una ventana de la webapp (diálogo modal con título) y el panel de atrás queda inerte',await p.evaluate(()=>{const b=document.getElementById('wm_box');return b.getAttribute('role')==='dialog'&&b.getAttribute('aria-modal')==='true'&&document.getElementById('wm_t').textContent.includes('SonosWiFi-A101')&&document.getElementById('bdrawer').inert&&b.contains(document.activeElement);}));
  await p.waitForSelector('#wm_list .wnet');
  ok('Configurar: busca solo las redes WiFi al abrirse (5 GHz primero, con banda y señal)',await p.evaluate(()=>{const l=[...document.querySelectorAll('#wm_list .wnet')];return l.length===4&&/Casa-5G/.test(l[0].textContent)&&/5 GHz/.test(l[0].textContent)&&/Señal/.test(l[0].textContent)&&/abierta/.test(l[3].textContent)&&l.every(x=>x.getBoundingClientRect().height>=43.5);}));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_configurar_altavoz.png'});
  await p.click('#wm_list .wnet:nth-child(3)');
  ok('Configurar: al tocar una red de 2,4 GHz se rellena el nombre y avisa que 5 GHz va mejor',(await p.inputValue('#wm_ssid'))==='Casa-2G'&&/2,4 GHz/.test(await p.textContent('#wm_msg'))&&(await p.getAttribute('#wm_list .wnet:nth-child(3)','aria-selected'))==='true');
  await p.click('#wm_list .wnet:nth-child(1)');
  ok('Configurar: al tocar una de 5 GHz se rellena el nombre, sin aviso, y el foco pasa a la contraseña',(await p.inputValue('#wm_ssid'))==='Casa-5G'&&!(await p.textContent('#wm_msg')).includes('2,4')&&await p.evaluate(()=>document.activeElement.id==='wm_pass'));
  await p.fill('#wm_ssid','');await p.click('#wm_go');
  ok('Configurar: sin nombre de red avisa',(await p.textContent('#wm_err')).includes('Elige una red'));
  await p.fill('#wm_ssid','Casa5G');await p.click('#wm_go');
  ok('Configurar: sin contraseña (y sin una guardada) la pide',(await p.textContent('#wm_err')).includes('Escribe la contraseña'));
  await p.fill('#wm_pass','corta');await p.click('#wm_go');
  ok('Configurar: contraseña corta -> error visible',(await p.textContent('#wm_err')).includes('8 y 63'));
  await p.click('#wm_show');
  ok('Configurar: «Mostrar» enseña la contraseña y «Ocultar» la vuelve a tapar',await p.evaluate(()=>document.getElementById('wm_pass').type==='text'&&document.getElementById('wm_show').textContent==='Ocultar')&&(await p.click('#wm_show'),await p.evaluate(()=>document.getElementById('wm_pass').type==='password')));
  fs.writeFileSync('/tmp/sonos_ble_mock.log','');
  await p.fill('#wm_pass','clave12345');await p.click('#wm_go');
  ok('Configurar: mientras trabaja muestra el avance y no se puede cerrar con Esc',await until(async()=>(await p.textContent('#wm_t')).includes('Configurando')&&(await p.textContent('#wm_prog')).length>3)&&(await p.keyboard.press('Escape'),await p.evaluate(()=>!document.getElementById('wm').hidden)));
  await p.waitForSelector('#wm_done',{timeout:30000});
  ok('Configurar: envía la red guardada y el ID automático (255) al altavoz',(()=>{const l=fs.readFileSync('/tmp/sonos_ble_mock.log','utf8').trim().split('\n').filter(Boolean).map(x=>JSON.parse(x)).filter(x=>x.cmd==='provision');return l.some(x=>x.address.endsWith('01')&&x.id===255&&x.ssid==='Casa5G'&&x.pass_len===10);})());
  ok('Configurar: vuelve a escanear por Bluetooth y dice que el altavoz NO se unió (el simulado sigue sin WiFi) con el motivo probable',await p.evaluate(()=>{const t=document.getElementById('wm_body').textContent;return document.getElementById('wm_t').textContent==='Resultado'&&t.includes('No se conectó')&&t.includes('No pudo unirse a «Casa5G»')&&t.includes('2,4 GHz');}));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_configurar_resultado.png'});
  await p.click('#wm_retry');
  ok('Configurar: «Reintentar» vuelve al formulario con la red anterior y la contraseña guardada reutilizable',await p.evaluate(()=>document.getElementById('wm_ssid').value==='Casa5G'&&/Guardada/.test(document.getElementById('wm_pass').placeholder)&&document.getElementById('wm_t').textContent.includes('SonosWiFi-A101')));
  await p.click('#wm_cancel');
  ok('Configurar: «Cancelar» cierra la ventana y devuelve el panel de atrás a la vida',await p.evaluate(()=>document.getElementById('wm').hidden&&!document.getElementById('bdrawer').inert));
  await p.click('.found[data-i="2"] [data-pv]');await p.waitForSelector('#wm:not([hidden])');
  await p.click('#wm_go');
  await p.waitForSelector('#wm_done',{timeout:30000});
  ok('Configurar: si el altavoz se une (el simulado ya tenía WiFi) lo confirma con red e IP y ofrece «Listo»',await p.evaluate(()=>{const t=document.getElementById('wm_body').textContent;return t.includes('Conectado')&&t.includes('Se unió a «Casa5G»')&&t.includes('192.168.1.52')&&!document.getElementById('wm_retry')&&document.getElementById('wm_done').textContent==='Listo';}));
  await p.click('#wm_done');
  await p.waitForSelector('#b_all');
  await p.click('#b_all');await p.waitForSelector('#wm:not([hidden])');
  ok('Configurar los N nuevos: una sola ventana para todos',(await p.textContent('#wm_t')).includes('3 altavoces'));
  await p.click('#wm_go');await p.waitForSelector('#wm_done',{timeout:40000});
  ok('Configurar los N nuevos: se ve el resultado de cada uno y el motivo del que falló',await p.evaluate(()=>{const t=document.getElementById('wm_body').textContent;return t.includes('emparejamiento rechazado')&&t.includes('SonosWiFi-A101')&&t.includes('SonosWiFi-B202');}));
  const lines=fs.readFileSync('/tmp/sonos_ble_mock.log','utf8').trim().split('\n').map(l=>JSON.parse(l)).filter(x=>x.cmd==='provision');
  ok('BLE: se configuraron A101 y B202 con la WiFi guardada y ID automático (255)',lines.some(x=>x.address.endsWith('01')&&x.id===255&&x.ssid==='Casa5G'&&x.pass_len===10)&&lines.some(x=>x.address.endsWith('02')&&x.id===255),lines);
  await p.keyboard.press('Escape');
  ok('Esc cierra primero la ventana de Configurar y el panel Bluetooth sigue abierto',await p.evaluate(()=>document.getElementById('wm').hidden&&document.getElementById('bdrawer').classList.contains('on')));
  await p.keyboard.press('Escape');ok('Esc cierra el panel Bluetooth',await until(async()=>!(await p.$('#bdrawer.on'))));
  ok('panel sin errores de JS',errs.length===0,errs);
  // ── PWA ──
  const mf=await fetch(BASE+'/manifest.webmanifest');const mj=await mf.json();
  ok('PWA: manifiesto válido (nombre, standalone, inicio, colores, 3 iconos, atajos)',mj.name==='AStream'&&mj.display==='standalone'&&mj.start_url==='/'&&mj.theme_color==='#1c1c1e'&&mj.background_color==='#1c1c1e'&&mj.lang==='es'&&mj.icons.length>=3&&mj.icons.some(i=>i.purpose==='maskable')&&mj.shortcuts.length>=2,mj);
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
  const room0=await p.evaluate(()=>({r:document.getElementById('room').getBoundingClientRect().width,m:document.querySelector('main').getBoundingClientRect().width,s:document.documentElement.scrollWidth}));
  await p.click('#spks .spk[data-id="2"]');await p.waitForSelector('#drawer.on');await adv(p);await sleep(500);
  const dk=await p.evaluate((r0)=>{const d=document.getElementById('drawer').getBoundingClientRect();const o={room:document.getElementById('room').getBoundingClientRect().width-r0.r,main:document.querySelector('main').getBoundingClientRect().width-r0.m,sw:document.documentElement.scrollWidth-r0.s,pr:parseFloat(getComputedStyle(document.querySelector('main')).paddingRight),dr:d.right-innerWidth,pos:getComputedStyle(document.getElementById('drawer')).position};o.ok=Math.abs(o.room)<1&&Math.abs(o.main)<1&&o.sw<=0&&o.pr<100&&o.dr<=0&&o.dr>=-24&&o.pos==='fixed';return o;},room0);
  ok('escritorio ancho: abrir el panel del altavoz NO cambia el tamaño de la vista (el plano y la página miden lo mismo), el panel flota arriba a la derecha',dk.ok===true,JSON.stringify(dk));
  ok('escritorio ancho: el cajón es un panel lateral fijo (sin velo, la página sigue activa)',await until(async()=>await p.evaluate(()=>document.body.classList.contains('has-dock')&&!document.getElementById('scrim').classList.contains('on')&&!document.querySelector('main').inert&&!document.querySelector('.top').inert&&!document.getElementById('drawer').inert)));
  await p.keyboard.press('Escape');
  ok('al cerrar con Esc: el foco vuelve al altavoz que lo abrió y la página deja de ser inerte',await until(async()=>await p.evaluate(()=>document.activeElement&&document.activeElement.classList.contains('spk')&&document.activeElement.dataset.id==='2'&&!document.querySelector('main').inert&&document.getElementById('drawer').inert)));
  await p.focus('.tab[data-tab="spk"]');await p.keyboard.press('ArrowRight');
  ok('pestañas con teclado: → selecciona y enfoca la siguiente (foco itinerante)',await p.evaluate(()=>document.activeElement.dataset.tab==='snd'&&document.activeElement.getAttribute('aria-selected')==='true'&&document.querySelector('.tab[data-tab="spk"]').tabIndex===-1&&!document.getElementById('p_snd').hidden));
  await p.keyboard.press('End');ok('pestañas con teclado: Fin va a la última',await p.evaluate(()=>document.activeElement.dataset.tab==='sys'));
  await p.keyboard.press('Home');ok('pestañas con teclado: Inicio va a la primera',await p.evaluate(()=>document.activeElement.dataset.tab==='spk'));
  ok('pestañas enlazadas con sus paneles (aria-controls / aria-labelledby)',await p.evaluate(()=>[...document.querySelectorAll('.tab')].every(t=>{const pane=document.getElementById(t.getAttribute('aria-controls'));return pane&&pane.getAttribute('aria-labelledby')===t.id;})));
  ok('los avisos se anuncian al lector de pantalla (región en vivo) y los errores como alerta',await p.evaluate(()=>{const tb=document.getElementById('tb');toast('prueba',false);const a=tb.querySelector('.toast.bad');const r=tb.getAttribute('role')==='status'&&tb.getAttribute('aria-live')==='polite'&&a&&a.getAttribute('role')==='alert';if(a)a.remove();return r;}));
  ok('volumen maestro con texto para lector de pantalla (aria-valuetext)',await p.evaluate(()=>/\d+ %/.test(document.getElementById('mvs').getAttribute('aria-valuetext')||'')));
  // Tipografía: Roboto autoalojada en toda la app
  ok('tipografía: Roboto 300/400/500/700 cargadas desde la RPi (sin internet)',await p.evaluate(async()=>{await document.fonts.ready;return [300,400,500,700].every(w=>document.fonts.check(w+' 16px Roboto'))&&[...document.fonts].filter(f=>f.family.replace(/"/g,'')==='Roboto'&&f.status==='loaded').length>=2;}));
  ok('tipografía: texto, botones, campos y números usan Roboto (ninguna fuente monoespaciada)',await p.evaluate(()=>{const sel=['body','.btn','input','.big','.lvn','.cal','.log','.cmd','h2','.tab'];return sel.every(q=>{const e=document.querySelector(q);return !e||/^"?Roboto"?/.test(getComputedStyle(e).fontFamily);});}));
  // ── Funciones nuevas: probar altavoz, distancias, modo noche, LFE, perfiles, salud ──
  await p.click('[data-tab=spk]');await p.click('#spks .spk[data-id="3"][data-side="0"]');await p.waitForSelector('#drawer.on');await adv(p);await sleep(300);
  ok('cajón: ESP estéreo ofrece Izquierdo, Derecho y Ambos para probar',await p.locator('#d_tone button').count()===3);
  ok('EQ: las perillas de las 6 bandas miden 18 px, no se pisan (≥4 px libres entre filas) y los demás deslizadores conservan los 26 px',await p.evaluate(()=>{const r=[...document.querySelectorAll('#dbody .eqrow input')];const th=parseFloat(getComputedStyle(r[0]).getPropertyValue('--th'));const vis=th+6;let min=1e9;for(let i=1;i<r.length;i++){const a=r[i-1].getBoundingClientRect(),b=r[i].getBoundingClientRect();min=Math.min(min,(b.top+b.height/2)-(a.top+a.height/2)-vis);}const vol=document.querySelector('#dbody input.rng:not(.mid)');return r.length===6&&th===18&&min>=4&&parseFloat(getComputedStyle(vol).getPropertyValue('--th'))===26;}));
  await p.evaluate(()=>document.getElementById('d_eq0').scrollIntoView({block:'center'}));await sleep(250);if(SHOTS)await p.screenshot({path:SHOTS+'/panel_eq.png'});
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
  await p.click('#spks .spk[data-id="4"][data-side="0"]');await p.waitForSelector('#drawer.on');await adv(p);await sleep(300);
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
  await p.click('.tab[data-tab="snd"]');await sleep(300);await p.click('#sb_mix');
  await p.click('#ntseg button[data-v="2"]');
  ok('modo noche: «Medio» llega al servidor y queda marcado',await until(async()=>(await J('/api/status')).night===2)&&await until(async()=>(await p.getAttribute('#ntseg button[data-v="2"]','aria-pressed'))==='true'));
  ok('modo noche: el texto explica el nivel elegido',(await p.textContent('#nth')).startsWith('Medio'));
  await p.click('#sw_lfe');
  ok('LFE en frontales: el interruptor llega al servidor',await until(async()=>(await J('/api/status')).lfe_front===true));
  await p.evaluate(()=>{const e=document.getElementById('lfes');e.value=4;e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));});
  ok('LFE: el nivel +4 dB llega al servidor',await until(async()=>(await J('/api/status')).lfe_db===4));
  await p.click('#sb_pf');await p.fill('#pfname','Cine noche');await p.click('#pfsave');
  ok('perfiles: guardar muestra el perfil en la lista',await until(async()=>(await p.locator('#pflist .pfrow').count())===1&&(await p.textContent('#pflist')).includes('Cine noche')));
  await p.click('#sb_mix');await p.click('#ntseg button[data-v="0"]');await p.click('#sw_lfe');await p.click('#sb_pf');
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
   await p.click('.tab[data-tab="sys"]');await p.click('#sy_rpi');
   ok('Raspberry: muestra la red actual y la versión instalada',await until(async()=>(await p.textContent('#rpi_cur')).includes('CasaHogar')&&(await p.textContent('#upd_ver')).length>0&&(await p.textContent('#upd_ver'))!=='—'));
   ok('Raspberry: sin contraseña avisa y deja cambiar la red y actualizar desactivados',await p.evaluate(()=>!document.getElementById('rpi_nopw').hidden&&document.getElementById('rpi_chg').disabled&&document.getElementById('upd_btn').disabled&&document.getElementById('rpi_go').disabled));
   // Tras «Actualizar servidor» la página se recarga sola cuando la versión cambia (aunque no se haya visto la caída del servicio)
   {await p.evaluate(()=>{window.__rl=0;sysVer0='viejaaa';sysBusy=true;sysSeenDown=false;sysReloading=false;});
    await p.route('**/api/system',r=>r.fulfill({contentType:'application/json',body:JSON.stringify({version:'nuevaaa',protected:true,wifi:{available:true,ssid:'CasaHogar',iface:'wlan0',state:'ok',msg:'',target:''},update:{state:'ok',log:''}})}));
    const nav=p.waitForNavigation({timeout:9000}).then(()=>true).catch(()=>false);
    ok('Actualizar servidor: al cambiar la versión la página se recarga sola',await nav);
    await p.unroute('**/api/system');await p.waitForSelector('.tab[data-tab="sys"]');await p.click('.tab[data-tab="sys"]');await p.click('#sy_rpi');}
   await p.click('.tab[data-tab="sys"]');await p.click('#sy_fw');await sleep(300);
   ok('firmware: avisa de flashear la primera vez por cable y ya no dice «sin probar»',(await p.textContent('#fwwarn')).includes('por cable')&&!(await p.textContent('#fwwarn')).toLowerCase().includes('sin probar'));
   await p.click('#fwup');
   ok('firmware: subir sin elegir archivo avisa',await p.locator('#tb .toast.bad').count()>=1);
   await p.evaluate(()=>document.querySelectorAll('#tb .toast').forEach(t=>t.remove()));
   await p.setInputFiles('#fwfile','/tmp/fw_ui_bad.bin');await p.click('#fwup');
   ok('firmware: un archivo que no es firmware se rechaza con un mensaje claro',await until(async()=>(await p.locator('#tb .toast.bad').allTextContents()).some(t=>t.includes('ESP-IDF'))));
   await p.evaluate(()=>document.querySelectorAll('#tb .toast').forEach(t=>t.remove()));
   await p.setInputFiles('#fwfile','/tmp/fw_ui_ok.bin');await p.click('#fwup');
   ok('firmware: un .bin válido se carga y se muestra versión, tamaño y chip',await until(async()=>{const t=await p.textContent('#fwinfo');return t.includes('9.9-ui')&&t.includes('78 KB')&&t.includes('C5');}),await p.textContent('#fwinfo'));
   {await p.evaluate(()=>{const n0=Object.assign({},nodes[0],{online:true,enabled:true,known:true,fw_version:7,ota_busy:false,chip:0});nodes=[n0];fw={loaded:true,size:1000,chip:0,version:'8'};drawFw();});
    await p.click('#fwlist [data-ota]');
    await p.evaluate(()=>{drawFw();drawFw();});await sleep(300);
    ok('firmware: «Actualizar» pide confirmación («¿Seguro?») y la conserva aunque el panel se redibuje con los refrescos',await p.evaluate(()=>/Seguro/.test(document.querySelector('#fwlist [data-ota]').textContent)));
    await p.evaluate(()=>{otaArm=null;otaPend=nodes[0].id;otaPendT=Date.now();drawFw();});
    ok('firmware: al confirmar, mientras se envía la orden ya muestra «Actualizando…» (botón desactivado) y no vuelve a «Actualizar»',await p.evaluate(()=>{const b=document.querySelector('#fwlist [data-ota]');return b.disabled&&b.textContent==='Iniciando…'&&document.getElementById('fwlist').textContent.includes('Actualizando…');}));
    await p.evaluate(()=>{drawFw();drawFw();});
    ok('firmware: los refrescos del panel no borran el estado «Actualizando…» mientras se envía',await p.evaluate(()=>document.getElementById('fwlist').textContent.includes('Actualizando…')&&document.querySelector('#fwlist [data-ota]').disabled));
    await p.evaluate(()=>{nodes[0].ota_busy=true;drawFw();});
    ok('firmware: cuando el altavoz informa que actualiza, pasa al «Actualizando…» real con «Cancelar»',await p.evaluate(()=>otaPend===null&&!!document.querySelector('#fwlist [data-ocancel]')&&document.getElementById('fwlist').textContent.includes('Actualizando…')));
    await p.evaluate(()=>{nodes[0].ota_busy=false;otaPend=nodes[0].id;otaPendT=Date.now()-21000;drawFw();});
    ok('firmware: si la orden no llega a verse en 20 s, el botón vuelve a «Actualizar»',await p.evaluate(()=>otaPend===null&&document.querySelector('#fwlist [data-ota]').textContent==='Actualizar'));
    await p.evaluate(()=>{otaArm=null;drawFw();});
    ok('firmware: sin confirmar, el botón vuelve a «Actualizar»',await p.evaluate(()=>document.querySelector('#fwlist [data-ota]').textContent==='Actualizar'));
    await p.evaluate(()=>{loadNodes();loadFw();});await until(async()=>await p.evaluate(()=>document.querySelectorAll('#fwlist [data-ota]').length===5&&[...document.querySelectorAll('#fwlist [data-ota]')].every(b=>b.disabled)));}
   ok('firmware: los ESP sin versión no se pueden actualizar por WiFi (botón desactivado y motivo visible)',await p.evaluate(()=>{const b=[...document.querySelectorAll('#fwlist [data-ota]')];return b.length===5&&b.every(x=>x.disabled)&&document.getElementById('fwlist').textContent.includes('primero por cable');}));
   if(SHOTS)await p.screenshot({path:SHOTS+'/panel_firmware.png',fullPage:true});}
  ok('nuevas funciones: sin errores de JS',errs.length===0,errs);
  await p.click('.tab[data-tab="spk"]');
  // Forma recta: todos los radios a 0 salvo el plano de la sala (altavoces, vúmetros, «Escucha»)
  {const rad=()=>p.evaluate(()=>{const out=[],inn=[];for(const e of document.querySelectorAll('body *')){if(e.closest('svg')&&!e.matches('svg'))continue;const r=parseFloat(getComputedStyle(e).borderTopLeftRadius)||0;const rb=parseFloat(getComputedStyle(e,'::before').borderTopLeftRadius)||0;const ra=parseFloat(getComputedStyle(e,'::after').borderTopLeftRadius)||0;
      if(e.closest('#room')&&e.id!=='room'&&!e.closest('.zoomctl'))inn.push(Math.max(r,rb,ra));else if(Math.max(r,rb,ra)>0)out.push((e.id||e.className||e.tagName)+':'+Math.max(r,rb,ra));}
    return {out:out.slice(0,8),nOut:out.length,inMax:Math.max(0,...inn),inCount:inn.filter(x=>x>0).length};});
   const r0=await rad();ok('forma: redonda (?forma=redonda) hay esquinas redondeadas fuera del plano',r0.nOut>20,r0);
   await p.evaluate(()=>SonosTheme.shape('square'));await sleep(250);
   const r1=await rad();
   ok('forma recta: ningún elemento fuera del plano conserva radio (botones, tarjetas, pestañas, interruptores, deslizadores, cajón)',r1.nOut===0&&await p.evaluate(()=>document.documentElement.getAttribute('data-shape')==='square'),r1);
   ok('forma recta: el dibujo del altavoz, su vúmetro y «Escucha» conservan sus formas redondeadas',r1.inCount>5&&r1.inMax>0&&await p.evaluate(()=>['#spks .cab *','#spks .vu'].every(q=>[...document.querySelectorAll(q)].some(e=>parseFloat(getComputedStyle(e).borderTopLeftRadius)>0))),[r0.inCount,r1.inCount,r0.inMax,r1.inMax]);
   ok('forma recta: el propio plano y los controles de zoom también quedan rectos',await p.evaluate(()=>[document.getElementById('room'),...document.querySelectorAll('.zoomctl,.zoomctl *')].every(e=>parseFloat(getComputedStyle(e).borderTopLeftRadius)===0)));
   if(SHOTS)await p.screenshot({path:SHOTS+'/recta_altavoces.png'});
   await p.click('.tab[data-tab="snd"]');await sleep(300);if(SHOTS)await p.screenshot({path:SHOTS+'/recta_sonido.png',fullPage:true});
   await p.click('.tab[data-tab="lat"]');await sleep(300);if(SHOTS)await p.screenshot({path:SHOTS+'/recta_latencia.png',fullPage:true});
   await p.click('#spks .spk[data-id="3"][data-side="0"]').catch(()=>{});await p.click('.tab[data-tab="spk"]');await p.click('#spks .spk[data-id="3"][data-side="0"]');await p.waitForSelector('#drawer.on');await adv(p);await sleep(400);
   ok('forma recta: el cajón del altavoz también es recto',await p.evaluate(()=>parseFloat(getComputedStyle(document.getElementById('drawer')).borderTopLeftRadius)===0));
   ok('forma recta: el recuadro de selección del altavoz abierto es cuadrado (también sus rótulos)',await p.evaluate(()=>{const b=document.querySelector('#spks .spk[aria-expanded=true]');if(!b)return false;const g=e=>parseFloat(getComputedStyle(e).borderTopLeftRadius);
     return g(b)===0&&getComputedStyle(b).outlineStyle==='solid'&&[...b.querySelectorAll('.cap,.cal')].every(e=>g(e)===0);}));
   ok('plano: fondo sólido (sin degradé) y del color del fondo en el tema por defecto (Grafito)',await p.evaluate(()=>{const r=getComputedStyle(document.getElementById('room'));return r.backgroundImage==='none'&&r.backgroundColor==='rgb(43, 43, 45)';}));
   if(SHOTS)await p.screenshot({path:SHOTS+'/recta_cajon.png'});
   await p.keyboard.press('Escape');await until(async()=>!(await p.$('#drawer.on')));
   await p.evaluate(()=>SonosTheme.shape('round'));await sleep(250);
   ok('forma redonda: el recuadro de selección vuelve a tener esquinas redondeadas',await p.evaluate(()=>{const b=document.querySelector('#spks .spk');return parseFloat(getComputedStyle(b).borderTopLeftRadius)>0;}));
   ok('forma redonda: el plano sigue sin degradé',await p.evaluate(()=>getComputedStyle(document.getElementById('room')).backgroundImage==='none'));
   ok('forma redonda: al volver, los botones vuelven a ser píldoras (999 px)',await p.evaluate(()=>parseFloat(getComputedStyle(document.querySelector('.btn')).borderTopLeftRadius)>=100));
   await p.goto(BASE+'/?forma=recta');await p.waitForSelector('#spks .spk',{state:'attached'});
   {const fc=await b.newContext({viewport:{width:1280,height:860}});const fp=await fc.newPage();await fp.goto(BASE+'/');await fp.waitForSelector('#spks .spk',{state:'attached'});
    ok('forma: por defecto es recta (sin radios en botones y tarjetas) en el tema por defecto',await fp.evaluate(()=>document.documentElement.getAttribute('data-shape')==='square'&&[...document.querySelectorAll('.btn,.card')].filter(e=>e.offsetParent).every(e=>parseFloat(getComputedStyle(e).borderTopLeftRadius)===0)));
    for(const t of ['graphite','light']){await fp.evaluate(t=>SonosTheme.set(t),t);await sleep(150);ok('forma: recta por defecto también en el tema '+t,await fp.evaluate(()=>[...document.querySelectorAll('.btn,.card')].filter(e=>e.offsetParent).every(e=>parseFloat(getComputedStyle(e).borderTopLeftRadius)===0)));
     ok('plano: fondo sólido (color liso, sin degradé) en el tema '+t,await fp.evaluate(()=>{const r=getComputedStyle(document.getElementById('room'));return r.backgroundImage==='none'&&r.backgroundColor===getComputedStyle(document.documentElement).getPropertyValue('--room-bg').trim().replace(/^#(..)(..)(..)$/,(m,x,y,z)=>'rgb('+[x,y,z].map(h=>parseInt(h,16)).join(', ')+')');}));}
    await fc.close();}
   ok('forma: ?forma=recta la activa y se recuerda; ?forma=redonda la quita',await p.evaluate(()=>SonosTheme.getShape()==='square')&&(await p.goto(BASE+'/?forma=redonda'),await p.waitForSelector('#spks .spk',{state:'attached'}),await p.evaluate(()=>SonosTheme.getShape()==='round'&&document.documentElement.getAttribute('data-shape')==='round')));
   await p.click('.tab[data-tab="spk"]');}
  // Móvil
  const m=await b.newContext({viewport:{width:390,height:844},deviceScaleFactor:2,isMobile:true,hasTouch:true});
  await m.addInitScript(()=>{try{if(localStorage.getItem('sonos_shape')===null)localStorage.setItem('sonos_shape','round');}catch(e){}});
  const q=await m.newPage();await q.goto(BASE+'/');await q.waitForSelector('#spks .spk');await sleep(800);
  ok('móvil: sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  if(SHOTS){await q.screenshot({path:SHOTS+'/movil_altavoces.png'});await q.locator('#meters').screenshot({path:SHOTS+'/movil_entrada_leds.png'});await q.locator('#room').scrollIntoViewIfNeeded();await q.locator('#room').screenshot({path:SHOTS+'/movil_plano_vumetros.png'});}
  ok('móvil: la navegación es una barra fija pegada abajo (sin flotar) con icono y una sola palabra por pestaña',await q.evaluate(()=>{const t=document.querySelector('.tabs'),r=t.getBoundingClientRect(),cs=getComputedStyle(t);
    return cs.position==='fixed'&&Math.abs(r.bottom-innerHeight)<1&&r.left<1&&Math.abs(r.right-innerWidth)<1&&parseFloat(cs.borderTopLeftRadius)===0&&[...t.querySelectorAll('.tab')].every(b=>b.querySelector('svg')&&getComputedStyle(b.querySelector('svg')).display!=='none'&&b.textContent.trim().split(/\s+/).length===1);}));
  ok('móvil: objetivos táctiles de 44 px como mínimo (botones, segmentos, pestañas, selector, campo)',await q.evaluate(()=>['#lble','#lcal','#medit','.tab','#spks .spk','#mvs'].every(s=>{const e=document.querySelector(s);const r=e.getBoundingClientRect();return r.height>=44-.5;})));
  ok('móvil: la cabecera ya no es fija (la navegación va abajo)',await q.evaluate(()=>getComputedStyle(document.querySelector('.top')).position==='static'));
  ok('móvil: ningún texto por debajo de 11 px',await q.evaluate(()=>{for(const e of document.querySelectorAll('body *')){if(!e.offsetParent&&getComputedStyle(e).position!=='fixed')continue;if(![...e.childNodes].some(n=>n.nodeType===3&&n.textContent.trim()))continue;if(parseFloat(getComputedStyle(e).fontSize)<11)return false;}return true;}));
  await q.tap('#spks .spk[data-id="0"]');await q.waitForSelector('#drawer.on');await adv(q);await sleep(400);
  ok('móvil: el cajón es modal (foco dentro, resto inerte, con velo)',await until(async()=>await q.evaluate(()=>document.activeElement&&document.activeElement.id==='dclose'&&document.querySelector('main').inert&&document.querySelector('.top').inert&&!document.getElementById('drawer').inert&&document.getElementById('scrim').classList.contains('on')&&!document.body.classList.contains('has-dock'))));
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_cajon.png'});
  ok('móvil: la hoja muestra el asa (grabber)',await q.evaluate(()=>getComputedStyle(document.querySelector('#drawer .grab')).display==='block'));
  ok('móvil: los botones de la hoja miden 44 px (cerrar, interruptor)',await q.evaluate(()=>{const x=document.getElementById('dclose').getBoundingClientRect();const sw=getComputedStyle(document.querySelector('#drawer .switch'),'::before');return x.height>=43.5&&x.width>=43.5&&sw.content!=='none';}));
  {const bx=await q.locator('#dtitle').boundingBox();const cx=bx.x+bx.width/2,cy=bx.y+bx.height/2;
   await q.mouse.move(cx,cy);await q.mouse.down();await q.mouse.move(cx,cy+60,{steps:4});await q.mouse.move(cx,cy+220,{steps:6});await q.mouse.up();
   ok('móvil: deslizar la hoja hacia abajo la cierra (como un sheet de iOS)',await until(async()=>!(await q.$('#drawer.on'))));
   await q.tap('#spks .spk[data-id="0"]');await q.waitForSelector('#drawer.on');await adv(q);await sleep(300);
   const bx2=await q.locator('#dtitle').boundingBox();await q.mouse.move(bx2.x+bx2.width/2,bx2.y+5);await q.mouse.down();await q.mouse.move(bx2.x+bx2.width/2,bx2.y+25,{steps:3});await q.mouse.up();await sleep(300);
   ok('móvil: un arrastre corto no la cierra (vuelve a su sitio)',!!(await q.$('#drawer.on'))&&await q.evaluate(()=>document.getElementById('drawer').style.transform===''));}
  await q.tap('#dclose');await q.click('.tab[data-tab="snd"]');await sleep(300);
  ok('móvil: Sonido sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_sonido.png'});
  await q.click('.tab[data-tab="snd"]');await q.click('#sb_mix');await q.evaluate(()=>window.scrollTo(0,document.getElementById('c_nt').getBoundingClientRect().top+scrollY-70));await sleep(300);
  const segFit=()=>q.evaluate(()=>[...document.querySelectorAll('.seg.block button')].filter(b=>b.offsetParent).map(b=>{const r=document.createRange();r.selectNodeContents(b);const t=r.getBoundingClientRect(),c=b.getBoundingClientRect();return (t.left>=c.left-0.5&&t.right<=c.right+0.5&&t.top>=c.top-0.5&&t.bottom<=c.bottom+0.5)?null:(b.textContent+' '+Math.round(t.left-c.left)+'/'+Math.round(c.right-t.right));}).filter(Boolean));
  for(const w of [390,360,320]){await q.setViewportSize({width:w,height:844});await sleep(250);const bad=await segFit();ok('móvil '+w+' px: el texto de «Apagado/Suave/Medio/Fuerte» queda dentro de su botón (sin punto que lo empuje)',bad.length===0,bad);}
  await q.setViewportSize({width:390,height:844});await sleep(250);
  ok('móvil: el botón activo de ancho completo no muestra el punto (display none)',await q.evaluate(()=>getComputedStyle(document.querySelector('#ntseg button[aria-pressed=true]'),'::before').display==='none'));
  ok('móvil: modo noche, LFE y perfiles sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  ok('móvil: los segmentos de modo noche miden 44 px',await q.evaluate(()=>[...document.querySelectorAll('#ntseg button')].every(b=>b.getBoundingClientRect().height>=43.5)));
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_sonido_nuevo.png'});
  await q.evaluate(()=>SonosTheme.shape('square'));await sleep(250);
  ok('móvil forma recta: sin radios fuera del plano y el texto de los segmentos sigue dentro',await q.evaluate(()=>[...document.querySelectorAll('.btn,.card,.seg,.seg button,.tab,.switch,.chip')].filter(e=>e.offsetParent).every(e=>parseFloat(getComputedStyle(e).borderTopLeftRadius)===0))&&(await segFit()).length===0);
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_recta_sonido.png'});
  await q.tap('.tab[data-tab="spk"]');await q.evaluate(()=>window.scrollTo(0,0));await sleep(300);if(SHOTS)await q.screenshot({path:SHOTS+'/movil_recta_altavoces.png'});
  await q.evaluate(()=>SonosTheme.shape('round'));await sleep(250);
  await q.tap('.tab[data-tab="spk"]');await q.tap('#spks .spk[data-id="0"]');await q.waitForSelector('#drawer.on');await adv(q);await sleep(400);
  ok('móvil: los botones de prueba miden 36 px o más y caben sin scroll horizontal',await q.evaluate(()=>[...document.querySelectorAll('#d_tone button')].every(b=>b.getBoundingClientRect().height>=35.5)&&document.documentElement.scrollWidth<=window.innerWidth+1));
  ok('EQ: las perillas de las 6 bandas miden 18 px, no se pisan (≥4 px libres entre filas) y los demás deslizadores conservan los 26 px',await q.evaluate(()=>{const r=[...document.querySelectorAll('#dbody .eqrow input')];const th=parseFloat(getComputedStyle(r[0]).getPropertyValue('--th'));const vis=th+6;let min=1e9;for(let i=1;i<r.length;i++){const a=r[i-1].getBoundingClientRect(),b=r[i].getBoundingClientRect();min=Math.min(min,(b.top+b.height/2)-(a.top+a.height/2)-vis);}const vol=document.querySelector('#dbody input.rng:not(.mid)');return r.length===6&&th===18&&min>=4&&parseFloat(getComputedStyle(vol).getPropertyValue('--th'))===26;}));
  await q.evaluate(()=>document.getElementById('d_eq0').scrollIntoView({block:'center'}));await sleep(250);if(SHOTS)await q.screenshot({path:SHOTS+'/movil_eq.png'});
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
  ok('BLE móvil: ya no hay paso para elegir número; el paso de la WiFi está bloqueado antes de conectar',await bp.evaluate(()=>!document.getElementById('s2')&&!document.getElementById('pills')&&document.getElementById('s3').classList.contains('lock')&&document.getElementById('s3').querySelector('.num2').textContent==='2'));
  if(SHOTS)await bp.screenshot({path:SHOTS+'/ble_inicio.png',fullPage:true});
  await bp.click('#bc');await bp.waitForSelector('#s3:not(.lock)');
  ok('BLE: estado legible tras conectar',(await bp.textContent('#k_wifi'))==='Conectado a la WiFi'&&(await bp.textContent('#k_rssi')).startsWith('Buena')&&(await bp.textContent('#k_ip'))==='192.168.1.55'&&(await bp.textContent('#k_rpi'))==='Sincronizado');
  ok('BLE: lee el SSID guardado',(await bp.inputValue('#ssid'))==='MiRed5G');
  await bp.fill('#ssid','Casa5G');await bp.fill('#pass','clave1234');
  if(SHOTS)await bp.screenshot({path:SHOTS+'/ble_conectado.png',fullPage:true});
  await bp.click('#save');await bp.waitForFunction(()=>window.__w.some(w=>w[0]==='cmd'));
  const w=await bp.evaluate(()=>window.__w);const dec=a=>String.fromCharCode(...a);
  ok('BLE: escribe ID automático (255), SSID, contraseña y «apply»',JSON.stringify(w.find(x=>x[0]==='id')[1])==='[255]'&&dec(w.find(x=>x[0]==='ssid')[1])==='Casa5G'&&dec(w.find(x=>x[0]==='pass')[1])==='clave1234'&&dec(w.find(x=>x[0]==='cmd')[1])==='apply',w);
  await bp.fill('#pass','corta');await bp.click('#save');
  ok('BLE: valida la contraseña corta',(await bp.textContent('#res')).includes('8 y 63'));
  ok('BLE sin errores de JS',berr.length===0,berr);
  // ── Nombres de las 10 entradas ──
  await p.click('#tab_sys');await p.click('#sy_in');await sleep(200);
  ok('Nombres: tarjeta con 10 campos, con los nombres de siempre por defecto',await p.evaluate(()=>{const v=[...document.querySelectorAll('#nmgrid input')].map(i=>i.value).join();return v==='FL,FR,C,LFE,SL,SR,SBL,SBR,AUX-L,AUX-R';}));
  ok('Nombres: cada campo tiene etiqueta visible con su número y su función (≥ 44 px de alto)',await p.evaluate(()=>[...document.querySelectorAll('#nmgrid input')].every((i,k)=>{const l=document.querySelector('label[for="'+i.id+'"]');return l&&l.textContent.startsWith('A'+(k+1))&&i.getBoundingClientRect().height>=44&&i.maxLength===12;})));
  await p.fill('#chn_0','Sala TV');await p.fill('#chn_3','Sub');await p.fill('#chn_9','');
  await p.click('#nmsave');await sleep(500);
  const nmApi=await J('/api/names');
  ok('Nombres: Guardar los envía a la Raspberry (el vacío vuelve al de fábrica)',nmApi.names[0]==='Sala TV'&&nmApi.names[3]==='Sub'&&nmApi.names[9]==='AUX-R',nmApi);
  await p.click('#tab_spk');await sleep(300);
  ok('Nombres: los indicadores de entrada, el plano y las fichas usan el nombre nuevo',await p.evaluate(()=>{const labs=[...document.querySelectorAll('#mb .lab')].map(e=>e.textContent);const chips=document.body.innerText;return labs[0]==='Sala TV'&&labs[3]==='Sub'&&labs[9]==='AUX-R'&&[...document.querySelectorAll('#spks .spk')].some(b=>b.textContent.includes('Sala TV'))&&chips.includes('Sala TV');}));
  await p.click('#tab_sys');await p.click('#sy_in');await sleep(200);
  await p.fill('#chn_1','<b>x');await p.click('#nmsave');await sleep(400);
  ok('Nombres: un nombre con < se rechaza con aviso y no cambia nada',(await J('/api/names')).names[1]==='FR'&&await p.evaluate(()=>[...document.querySelectorAll('.toast')].some(t=>t.textContent.includes('<'))));
  if(SHOTS)await p.screenshot({path:SHOTS+'/panel_nombres.png'});
  await p.fill('#chn_1','');await p.click('#nmreset');await sleep(500);
  ok('Nombres: Restablecer devuelve los 10 nombres de fábrica en la interfaz y en la Raspberry',await p.evaluate(()=>[...document.querySelectorAll('#nmgrid input')].map(i=>i.value).join()==='FL,FR,C,LFE,SL,SR,SBL,SBR,AUX-L,AUX-R'&&document.querySelector('#mb .lab').textContent==='FL')&&(await J('/api/names')).names.join()==='FL,FR,C,LFE,SL,SR,SBL,SBR,AUX-L,AUX-R');
  await p.click('#tab_spk');await sleep(200);

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
  ok('Tema: por defecto Grafito (gris, no negro; theme-color #1c1c1e)',c.theme==='graphite'&&c.bg==='rgb(28, 28, 30)'&&c.meta==='#1c1c1e'&&c.cs==='dark',c);
  await p.click('.tab[data-tab="sys"]');await p.click('#sy_ui');
  ok('Interfaz (Sistema): el tema por defecto está marcado (Grafito) y la cabecera ya no tiene botón de tema',await p.evaluate(()=>document.querySelector('#ui_theme [data-v=graphite]').getAttribute('aria-pressed')==='true'&&!document.querySelector('.top [data-theme-btn]')));
  const thSeq=[];for(const v of ['dark','light','auto','graphite']){await p.click('#ui_theme [data-v='+v+']');thSeq.push(await p.evaluate(()=>[localStorage.getItem('sonos_theme'),document.querySelector('#ui_theme [aria-pressed=true]').dataset.v].join('/')));}
  ok('Interfaz: Grafito, Oscuro, Claro y Auto se eligen con un toque, se marcan y se guardan en este navegador',JSON.stringify(thSeq)==='["dark/dark","light/light","auto/auto","graphite/graphite"]',thSeq);
  await p.click('#ui_shape [data-v=round]');
  ok('Interfaz: «Curvos» cambia la forma al instante y se guarda (sonos_shape)',await p.evaluate(()=>document.documentElement.dataset.shape==='round'&&localStorage.getItem('sonos_shape')==='round'&&document.querySelector('#ui_shape [data-v=round]').getAttribute('aria-pressed')==='true'));
  await p.click('#ui_shape [data-v=square]');
  ok('Interfaz: «Rectos» vuelve a bordes rectos',await p.evaluate(()=>document.documentElement.dataset.shape==='square'&&localStorage.getItem('sonos_shape')==='square'));
  const szs={};for(const v of ['s','m','l']){await p.click('#ui_size [data-v='+v+']');szs[v]=await p.evaluate(()=>[parseFloat(getComputedStyle(document.documentElement).fontSize),document.documentElement.dataset.size,localStorage.getItem('sonos_size'),document.documentElement.scrollWidth<=innerWidth+1]);}
  ok('Interfaz: el tamaño del texto (pequeño < normal < grande) cambia la letra, se guarda y no desborda',szs.s[0]<szs.m[0]&&szs.m[0]<szs.l[0]&&szs.l[1]==='l'&&szs.l[2]==='l'&&szs.s[3]&&szs.m[3]&&szs.l[3],szs);
  await p.click('#ui_size [data-v=m]');
  const wz=await p.evaluate(()=>{const b=document.getElementById('ui_wiz');return b.getAttribute('aria-checked');});
  await p.click('#ui_wiz');
  ok('Interfaz: el interruptor «Guía de primeros pasos» se apaga y lo recuerda (sonos_wiz=off)',wz==='true'&&await p.evaluate(()=>document.getElementById('ui_wiz').getAttribute('aria-checked')==='false'&&localStorage.getItem('sonos_wiz')==='off'));
  await p.click('#ui_theme [data-v=light]');await p.click('#ui_shape [data-v=round]');await p.click('#ui_size [data-v=l]');
  await p.click('#ui_reset');
  ok('Interfaz: «Restablecer apariencia» vuelve a Grafito, rectos, texto normal y guía activa',await p.evaluate(()=>document.documentElement.dataset.theme==='graphite'&&document.documentElement.dataset.shape==='square'&&document.documentElement.dataset.size==='m'&&localStorage.getItem('sonos_theme')===null&&localStorage.getItem('sonos_wiz')===null&&document.getElementById('ui_wiz').getAttribute('aria-checked')==='true'));
  await p.click('#ui_theme [data-v=dark]');await p.click('#ui_shape [data-v=round]');await p.click('.tab[data-tab="spk"]');await p.reload();await p.waitForSelector('.spk');
  ok('Interfaz: tema y forma elegidos persisten al recargar',await p.evaluate(()=>document.documentElement.dataset.theme==='dark'&&document.documentElement.dataset.shape==='round'));
  await p.evaluate(()=>SonosTheme.reset());await p.reload();await p.waitForSelector('.spk');
  const noPurple=x=>['ac','bad','ok','warn'].every(k=>{const h=x[k].replace('#','');const r=parseInt(h.substr(0,2),16),g=parseInt(h.substr(2,2),16),b=parseInt(h.substr(4,2),16);return !(b>r&&r>g&&b>150&&r>100);});
  for(const [t,meta,cs] of [['graphite','#1c1c1e','dark'],['dark','#000000','dark'],['light','#d5d9e0','light']]){
    c=await setTheme(t);
    ok('Tema '+t+': data-theme, theme-color y color-scheme correctos',c.theme===t&&c.meta===meta&&c.cs===cs,c);
    ok('Tema '+t+': texto/fondo y texto atenuado ≥ 4,5:1',cr(c.fg,c.bg)>=4.5&&cr(c.mut,c.bg)>=4.5,[cr(c.fg,c.bg),cr(c.mut,c.bg)]);
    // Excepción decidida por el usuario: en «light» el acento es blanco (#fff) y los estados activos son un recuadro gris oscuro translúcido;
    // el blanco sobre ese recuadro (~2,2:1) y el blanco sobre el fondo no cumplen 4,5:1. Se comprueba que siga siendo blanco y que ok/aviso/error cumplan.
    if(t==='light'){
      ok('Tema light: ok, aviso y error ≥ 4,5:1 sobre el fondo; el acento es blanco por decisión de diseño (excepción documentada)',['ok','warn','bad'].every(k=>cr(rgbOf(c[k]),c.bg)>=4.5)&&rgbOf(c.ac)==='rgb(255,255,255)',['ok','warn','bad'].map(k=>[k,cr(rgbOf(c[k]),c.bg)]).concat([['ac',c.ac]]));
      const bt=await p.evaluate(()=>{const e=document.getElementById('go_proc'),c=getComputedStyle(e);return {fg:c.color.replace(/ /g,''),bg:c.backgroundColor.replace(/ /g,''),bd:c.borderTopWidth};});
      ok('Tema light: el botón principal (p. ej. «Guardar WiFi») se lee: texto oscuro sobre fondo blanco ≥ 4,5:1 y con borde',cr(bt.fg,bt.bg)>=4.5&&bt.fg!==bt.bg&&parseFloat(bt.bd)>=1,bt);
    }else{
    ok('Tema '+t+': acento y estados (ok, aviso, error) ≥ 4,5:1 sobre el fondo',['ac','ok','warn','bad'].every(k=>cr(rgbOf(c[k]),c.bg)>=4.5),['ac','ok','warn','bad'].map(k=>[k,cr(rgbOf(c[k]),c.bg)]));
    ok('Tema '+t+': texto del botón primario sobre el acento ≥ 4,5:1',cr(rgbOf(c.onac),rgbOf(c.ac))>=4.5,[c.onac,c.ac]);}
    ok('Tema '+t+': sin morado en los acentos',noPurple(c),c);
    ok('Tema '+t+': persiste al recargar (sin destello)',await p.evaluate(()=>localStorage.getItem('sonos_theme'))===t&&c.theme===t);
    if(SHOTS)await p.screenshot({path:SHOTS+'/tema_'+t+'_panel.png',fullPage:false});
  }
  c=await setTheme('light');
  ok('Tema claro suave: ni el fondo ni las tarjetas son blanco (luminancia < 0,72 y < 0,80)',lum(c.bg)<0.72&&lum(rgbOf(c.panel))<0.80&&lum(c.bg)>0.4,[lum(c.bg),lum(rgbOf(c.panel))]);
  c=await setTheme('graphite');
  ok('Tema Grafito: gris, no negro puro (fondo entre 0,008 y 0,05 de luminancia)',lum(c.bg)>0.008&&lum(c.bg)<0.05,lum(c.bg));
  await p.emulateMedia({colorScheme:'dark'});c=await setTheme('auto');
  ok('Tema Automático: sigue al sistema (aquí oscuro -> Grafito)',(await p.evaluate(()=>SonosTheme.get()))==='auto'&&c.theme==='graphite',c);
  await p.emulateMedia({colorScheme:'light'});await sleep(200);c=await colors();
  ok('Tema Automático: cambia a claro cuando el sistema pasa a claro',c.theme==='light',c);
  await p.emulateMedia({colorScheme:'dark'});await sleep(200);c=await colors();
  ok('Tema Automático: vuelve a Grafito',c.theme==='graphite',c);
  for(const u of ['/calibrate','/ble']){await p.evaluate(()=>localStorage.setItem('sonos_theme','graphite'));const q=await ctx.newPage();await q.goto(BASE+u);ok('Tema Grafito compartido en '+u,await q.evaluate(()=>document.documentElement.dataset.theme)==='graphite'&&await q.locator('[data-theme-btn]').count()===1);await q.close();}
  // botones de cabecera: todos con la misma altura y alineados (antes «Calibrar» era un enlace desplazado)
  await p.evaluate(()=>localStorage.removeItem('sonos_theme'));await p.reload();await p.waitForSelector('.spk');
  const hb=await p.evaluate(()=>['#lble','#lcal','#medit'].map(s=>{const r=document.querySelector(s).getBoundingClientRect();return [r.top,r.height];}));
  ok('Plano: Añadir altavoces (+), Calibrar y Editar plano están juntos, alineados y con la misma altura',hb.every(x=>Math.abs(x[0]-hb[0][0])<1&&Math.abs(x[1]-hb[0][1])<1),hb);
  ok('Plano: los botones quedan pegados a la derecha de la barra y el texto de ayuda va debajo, alineado a la izquierda',await p.evaluate(()=>{const bar=document.querySelector('.roombar').getBoundingClientRect(),e=document.getElementById('medit').getBoundingClientRect(),l=document.getElementById('lble').getBoundingClientRect(),h=document.getElementById('mhint').getBoundingClientRect();return Math.abs(bar.right-e.right)<2&&l.left>bar.left+bar.width/3&&h.top>=e.bottom-1&&Math.abs(h.left-bar.left)<2;}));
  ok('Estado de cada altavoz: cuenta los cortes de los últimos minutos y no los acumulados (un altavoz que ya anda bien vuelve a «Bien»)',await p.evaluate(()=>{const b={enabled:true,online:true,rssi_dbm:-60},v=x=>verdict(Object.assign({},b,x)).k;
    return v({lost_packets:900,late_packets:90,underruns:40,recent_min:10,recent_lost:0,recent_late:0,recent_underruns:0})==='ok'
      &&v({lost_packets:900,recent_min:10,recent_lost:2})==='warn'&&v({recent_min:10,recent_lost:80})==='bad'
      &&v({lost_packets:900})==='bad'&&verdict(Object.assign({},b,{recent_min:10})).why.includes('10 min');}));
  {await p.click('.tab[data-tab="lat"]');
   ok('Latencia: por defecto el modo es Manual (slider y botones de margen visibles, ajustes de Auto ocultos)',await p.evaluate(()=>document.querySelector('#plmode [data-m=manual]').getAttribute('aria-pressed')==='true'&&!document.getElementById('plmanual').hidden&&document.getElementById('plauto').hidden));
   await p.click('#plmode [data-m=auto]');
   ok('Latencia: al elegir Auto aparecen margen actual, mínimo y tope, y se ocultan los controles manuales',await until(async()=>await p.evaluate(()=>!document.getElementById('plauto').hidden&&document.getElementById('plmanual').hidden&&document.querySelector('#plmode [data-m=auto]').getAttribute('aria-pressed')==='true'&&/\d+ ms/.test(document.getElementById('plcur').textContent)&&+document.getElementById('plmax').value>=+document.getElementById('plfloor').value)));
   ok('Latencia Auto: muestra los umbrales con sus valores recomendados (8 cortes por minuto, 10 min, 10 ms)',await p.evaluate(()=>['plre','plcm','plst'].map(i=>document.getElementById(i).value).join()==='8,10,10'&&getComputedStyle(document.getElementById('plreset')).display!=='none'));
   await p.evaluate(()=>{const e=document.getElementById('plre');e.value=20;e.dispatchEvent(new Event('input'));e.dispatchEvent(new Event('change'));});
   ok('Latencia Auto: cambiar el umbral de cortes se guarda y «Valores recomendados» lo restaura',await until(async()=>(await (await fetch(BASE+'/api/latency')).json()).auto.raise_events===20));
   await p.click('#plreset');
   ok('Latencia Auto: «Valores recomendados» vuelve a 8, 10 y 10',await until(async()=>{const a=(await (await fetch(BASE+'/api/latency')).json()).auto;return a.raise_events===8&&a.clean_min===10&&a.step_ms===10;}));
   ok('Latencia Auto: explica el estado y el efecto de cada cambio',await until(async()=>(await p.textContent('#plautomsg')).includes('Auto activo')&&(await p.textContent('#p_lat')).includes('breve salto')));
   await p.evaluate(()=>{const e=document.getElementById('plmax');e.value=120;e.dispatchEvent(new Event('input'));e.dispatchEvent(new Event('change'));});
   ok('Latencia Auto: cambiar el tope se guarda en la Raspberry',await until(async()=>{const j=await (await fetch(BASE+'/api/latency')).json();return j.auto.on&&j.auto.max_ms===120;}));
   await p.click('#plmode [data-m=manual]');
   ok('Latencia: volver a Manual restaura los controles manuales',await until(async()=>await p.evaluate(()=>!document.getElementById('plmanual').hidden&&document.getElementById('plauto').hidden)));
   await fetch(BASE+'/api/playout',{method:'POST',body:JSON.stringify({ms:80})});await p.click('.tab[data-tab="spk"]');}
  ok('Calibrar no se subraya como enlace',await p.evaluate(()=>getComputedStyle(document.getElementById('lcal')).textDecorationLine)==='none');

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
  ok('Plano: «Editar plano» activa el modo (aria-pressed, botón Listo, restablecer visible)',await p.getAttribute('#medit','aria-pressed')==='true'&&(await p.textContent('#medit'))==='Listo'&&await p.isVisible('#mreset'));
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
  await ctx2.addInitScript(()=>{try{if(localStorage.getItem('sonos_shape')===null)localStorage.setItem('sonos_shape','round');}catch(e){}});
  const mp=await ctx2.newPage();await mp.goto(BASE+'/');await mp.waitForSelector('.spk');
  for(const t of ['graphite','light']){await mp.evaluate(t=>localStorage.setItem('sonos_theme',t),t);await mp.reload();await mp.waitForSelector('.spk');
    ok('Móvil: tema '+t+' sin desbordamiento horizontal',await mp.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));}
  ok('Móvil vertical: la cabecera es una fila (marca, estado y volumen) sin menú «⋯», sin botón de tema y sin Calibrar ni Añadir altavoces',await mp.evaluate(()=>!document.getElementById('more')&&!document.querySelector('.top [data-theme-btn]')&&!document.querySelector('.top #lcal')&&!document.querySelector('.top #lble')));
  const mh=await mp.evaluate(()=>['#lble','#lcal','#medit'].map(s=>{const r=document.querySelector(s).getBoundingClientRect();return [r.top,r.height,r.right,r.width];}));
  ok('Móvil vertical: +, Calibrar y Editar plano están junto al plano, miden ≥44 px y caben en pantalla',mh.every(x=>x[1]>=43.5&&x[2]<=390&&x[3]>0),mh);
  ok('Móvil vertical: «Añadir altavoces» es solo el signo + (con nombre accesible) y el texto de ayuda va debajo de los botones',await mp.evaluate(()=>{const b=document.getElementById('lble'),h=document.getElementById('mhint'),m=document.getElementById('medit').getBoundingClientRect(),hr=h.getBoundingClientRect(),br=b.getBoundingClientRect();return b.textContent.trim()===''&&b.getAttribute('aria-label')==='Añadir altavoces'&&br.width>=43.5&&br.width<=48&&hr.top>=Math.max(m.bottom,br.bottom)-1&&document.getElementById('medit').textContent==='Editar plano';}));
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
  ok('Móvil: tocar «+» dos veces acerca a 156 % sin cambiar el tamaño de altavoces ni de «Escucha»; el plano ya no bloquea el scroll de la página (touch-action pan-y)',zm1.z==='156%'&&Math.abs(zm1.w-zm0.w)<0.6&&Math.abs(zm1.h-zm0.h)<0.6&&zm1.mw===zm0.mw&&zm1.ta==='pan-y'&&zm0.ta==='pan-y',[zm0,zm1]);
  if(SHOTS){await mp.screenshot({path:SHOTS+'/movil_zoom_mas.png'});}
  // Gestos reales (CDP): con zoom un dedo mueve el plano mientras pueda; en el borde, el mismo gesto desliza la página
  {const cdp=await ctx2.newCDPSession(mp);
   const free=async()=>mp.evaluate(()=>{const r=document.getElementById('room').getBoundingClientRect();for(let y=Math.max(r.top+20,20);y<Math.min(r.bottom-20,innerHeight-120);y+=14)for(let x=r.left+8;x<r.right-8;x+=14){const e=document.elementFromPoint(x,y);if(e&&e.closest('#room')&&!e.closest('.spk,.zoomctl'))return {x,y};}return null;});
   const swipe=async(x,y0,dy)=>{await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y:y0}]});for(let i=1;i<=8;i++){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x,y:y0+dy*i/8}]});await sleep(20);}await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await sleep(450);};
   const st=()=>mp.evaluate(()=>({sy:scrollY,py:panY,max:ly_max()}));
   await mp.evaluate(()=>{window.ly_max=()=>{const {W,H}=vw(),[lx,ly]=LXY(W,H);return [(H-ly)*(1-zoom),ly*(zoom-1)];};});
   const fp0=await free();ok('Móvil gestos: hay un punto libre del plano para arrastrar',!!fp0,fp0);
   if(fp0){
    const a0=await st();await swipe(fp0.x,fp0.y,60);const a1=await st();
    ok('Móvil gestos (zoom 156 %): un dedo hacia abajo mueve el plano y NO desliza la página',a1.py>a0.py+20&&a1.sy===a0.sy,[a0,a1]);
    for(let i=0;i<14&&(await st()).py<(await st()).max[1]-1;i++){const f=(await free())||fp0;await swipe(f.x,f.y,80);}   // el punto se recalcula cada vez: al mover el plano un altavoz podría quedar bajo el dedo y arrastrarse
    const b0=await st();
    ok('Móvil gestos: con el plano ya en su borde superior (y la página desplazada)',b0.py>=b0.max[1]-1&&b0.sy>0,b0);
    const fb=(await free())||fp0;await swipe(fb.x,fb.y,80);const b1=await st();   // el punto se recalcula: la página se movió y uno viejo podría caer sobre un altavoz
    ok('Móvil gestos: en el borde, el mismo gesto desliza la página hacia arriba (el scroll ya no queda atrapado)',b1.sy<b0.sy-20&&Math.abs(b1.py-b0.py)<1,[b0,b1]);
    await sleep(1200);await mp.evaluate(()=>window.scrollTo(0,scrollY));await sleep(300);   // que termine el desliz inercial de la página: durante él el navegador no deja cancelar el gesto
    const fc=(await free())||fp0;const c0=await st();await swipe(fc.x,fc.y,-60);const c1=await st();
    ok('Móvil gestos: arrastrar en sentido contrario vuelve a mover el plano (aún puede moverse hacia ese lado)',c1.py<c0.py-20&&Math.abs(c1.sy-c0.sy)<2,[c0,c1]);
   }}
  await mp.tap('#zval');await mp.tap('#zout');await mp.tap('#zout');await sleep(150);const zm2=await mg();
  ok('Móvil: alejar a 64 % achica los altavoces (≈ 64 %), oculta las etiquetas y se solapan bastantes menos (5 → ≤ 2)',zm2.z==='64%'&&zm2.w<zm0.w*0.75&&zm2.ov<zm0.ov&&zm2.ov<=2&&zm2.cap==='none',[zm0,zm2]);
  ok('Móvil 64 %: el vúmetro queda dentro del recuadro del altavoz',await mp.evaluate(()=>[...document.querySelectorAll('#spks .spk')].every(b=>{const r=b.getBoundingClientRect(),v=b.querySelector('.vu').getBoundingClientRect();return v.left>=r.left-0.5&&v.right<=r.right+0.5;})));
  if(SHOTS){await mp.screenshot({path:SHOTS+'/movil_zoom_menos.png'});}
  {const cdp=await ctx2.newCDPSession(mp);const fp=await mp.evaluate(()=>{const r=document.getElementById('room').getBoundingClientRect();for(let y=Math.max(r.top+20,20);y<Math.min(r.bottom-20,innerHeight-160);y+=14)for(let x=r.left+8;x<r.right-8;x+=14){const e=document.elementFromPoint(x,y);if(e&&e.closest('#room')&&!e.closest('.spk,.zoomctl'))return {x,y};}return null;});
   if(fp){await mp.evaluate(()=>window.scrollTo(0,0));await sleep(200);const s0=await mp.evaluate(()=>scrollY);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:fp.x,y:fp.y}]});for(let i=1;i<=8;i++){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:fp.x,y:fp.y-12*i}]});await sleep(20);}await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await sleep(450);
    ok('Móvil gestos (zoom 64 %): arrastrar sobre el plano desliza la página',(await mp.evaluate(()=>scrollY))>s0+20,[s0]);}}
  await mp.tap('#zval');await sleep(100);
  ok('Móvil: el control de zoom no tapa el plano ni se sale de la pantalla (≥ 44 px)',await mp.evaluate(()=>{const z=document.querySelector('.zoomctl').getBoundingClientRect(),r=document.getElementById('room').getBoundingClientRect();return z.right<=r.right&&z.top>=r.top&&z.height>=44&&innerWidth>=z.right;}));
  await fetch(BASE+'/api/node',{method:'POST',body:JSON.stringify({id:2,pos_auto:true})});
  await mp.close();
  ok('Panel sin errores de JS (tema y plano)',errs.length===0,errs);

  // /calibrate con resultado inyectado
  {// Placas de entrada: lista con «Usar», reloj principal excluyente, orden, guardado e ids de canal en medidores/selectores/nombres
   const up=await ctx.newPage();const uerr=[];up.on('pageerror',e=>uerr.push(String(e)));
   let saved=null;
   const ids=['A1','A2','A3','A4','A5','A6','A7','A8','A9','A10','B1','B2','B3','B4','B5','B6'];
   const names=['FL','FR','C','LFE','SL','SR','SBL','SBR','AUX-L','AUX-R','FL','FR','C','LFE','SL','SR'];
   await up.route('**/api/inputs',async r=>{
     if(r.request().method()==='POST'){saved=JSON.parse(r.request().postData());return r.fulfill({contentType:'application/json',body:'{"ok":true,"restarting":false}'});}
     r.fulfill({contentType:'application/json',body:JSON.stringify({devices:[
       {key:'Traktor',name:'Traktor Audio 10',channels:10,enabled:true,tag:'A',connected:true,card:'hw:0,0',max_channels:10,busy:true},
       {key:'Scarlett 18i20',name:'Scarlett 18i20',channels:8,enabled:false,tag:'B',connected:true,card:'hw:2,0',max_channels:18,busy:false}],
       cards:[{key:'USB Audio Device',name:'USB Audio Device',id:'hw:3,0',max_channels:2,busy:false}],clock:'',clock_effective:'Traktor',max_total:32,direct:true,
       running:{devices:[{key:'Traktor',online:true,first:0,underruns:0,master:true}],clock:'Traktor',total:10},restart_needed:false})});});
   await up.route('**/api/status',async r=>{const resp=await r.fetch();const j=await resp.json();j.ch_names=names;j.ch_ids=ids;j.input_channels=16;r.fulfill({response:resp,contentType:'application/json',body:JSON.stringify(j)});});
   await up.route('**/api/levels',async r=>{const resp=await r.fetch();const j=await resp.json();j.db=ids.map((_,i)=>i===11?-12:(j.db[i]===undefined?-90:j.db[i]));r.fulfill({response:resp,contentType:'application/json',body:JSON.stringify(j)});});
   await up.goto(BASE+'/');await up.waitForSelector('#spks .spk');await sleep(1500);
   ok('Placas: con 16 entradas hay 16 indicadores',await until(async()=>(await up.locator('#mb .mc').count())===16),await up.locator('#mb .mc').count());
   ok('Placas: con varias placas los indicadores llevan el id (A1…, B1…) y el canal con señal se enciende',await up.evaluate(()=>{const l=[...document.querySelectorAll('#mb .lab')].map(x=>x.textContent);return l[0]==='A1'&&l[10]==='B1'&&l[15]==='B6'&&document.getElementById('lp11').dataset.st==='sig';}));
   await up.click('.tab[data-tab="sys"]');await up.click('#sy_in');await sleep(500);
   ok('Placas: se listan la Traktor, la Scarlett (guardada) y la tarjeta detectada, más la opción de la Raspberry',await up.evaluate(()=>[...document.querySelectorAll('#usb_list .usbrow[data-i]')].map(r=>r.querySelector('b').textContent.trim().replace(/\s+/g,' ')).join('|')==='Traktor Audio 10 A|Scarlett 18i20 B|USB Audio Device C'&&!!document.getElementById('usb_clk_rpi')));
   ok('Placas: la Traktor no es obligatoria ni especial: no hay botón «Quitar» en placas conectadas y se puede desactivar',await up.evaluate(()=>!document.querySelector('#usb_list [data-rm]')&&document.querySelector('[data-use="0"]').getAttribute('aria-checked')==='true'));
   ok('Placas: con una sola placa activa su reloj es el principal (radio marcado) y sin usar quedan deshabilitados canales y reloj',await until(async()=>up.evaluate(()=>document.querySelector('[data-clk="0"]').checked&&!document.getElementById('usb_clk_rpi').checked&&document.querySelector('#usb_list select[data-ch="1"]').disabled&&document.querySelector('[data-clk="1"]').disabled)),await up.evaluate(()=>JSON.stringify({c0:document.querySelector('[data-clk="0"]').checked,rpi:document.getElementById('usb_clk_rpi').checked,d1:document.querySelector('#usb_list select[data-ch="1"]').disabled,k1:document.querySelector('[data-clk="1"]').disabled,rows:document.querySelectorAll('#usb_list .usbrow').length})));
   ok('Placas: la Traktor se ve con sus entradas (en línea, 1–10)',(await up.textContent('#usb_list .usbrow[data-i="0"]')).includes('En línea · entradas 1–10'));
   await up.click('[data-use="1"]');
   ok('Placas: al usar la Scarlett (8 canales) ocupa 11–18 y, con dos placas activas y reloj automático, manda la Raspberry',await up.evaluate(()=>(document.querySelector('#usb_list .usbrow[data-i="1"]').textContent.includes('entradas 11–18'))&&document.getElementById('usb_clk_rpi').checked&&!document.querySelector('[data-clk="0"]').checked&&!document.querySelector('[data-clk="1"]').disabled));
   await up.click('[data-clk="1"]');
   ok('Placas: el reloj es excluyente: al elegir la Scarlett se desmarcan la Traktor y la Raspberry',await up.evaluate(()=>document.querySelector('[data-clk="1"]').checked&&!document.querySelector('[data-clk="0"]').checked&&!document.getElementById('usb_clk_rpi').checked));
   await up.click('#usb_clk_rpi');
   ok('Placas: elegir la Raspberry desmarca las placas',await up.evaluate(()=>document.getElementById('usb_clk_rpi').checked&&!document.querySelector('[data-clk="0"]').checked&&!document.querySelector('[data-clk="1"]').checked));
   await up.click('[data-use="2"]');
   await up.selectOption('#usb_list select[data-ch="2"]','2');
   await up.click('[data-mv="1:-1"]');
   ok('Placas: «↑» sube la Scarlett y las entradas se renumeran (Scarlett 1–8, Traktor 9–18, USB 19–20)',await up.evaluate(()=>{const r=[...document.querySelectorAll('#usb_list .usbrow[data-i]')].map(x=>x.textContent);return /Scarlett/.test(r[0])&&r[0].includes('entradas 1–8')&&r[1].includes('Traktor')&&r[1].includes('entradas 9–18')&&r[2].includes('entradas 19–20');}));
   ok('Placas: se activa «Guardar y reiniciar»',!(await up.isDisabled('#usb_save')));
   await up.click('#usb_save');
   ok('Placas: guardar envía el orden, «Usar», canales, letras y el reloj elegido',await until(async()=>saved&&saved.clock==='rpi'&&saved.devices.length===3&&saved.devices[0].key==='Scarlett 18i20'&&saved.devices[0].enabled===true&&saved.devices[0].channels===8&&saved.devices[1].key==='Traktor'&&saved.devices[1].channels===10&&saved.devices[1].tag==='A'&&saved.devices[2].key==='USB Audio Device'&&saved.devices[2].enabled===true&&saved.devices[2].channels===2),JSON.stringify(saved));
   ok('Placas: tras guardar se avisa que hace falta reiniciar',await until(async()=>(await up.textContent('#usb_msg')).includes('Reiniciá')),await up.textContent('#usb_msg'));
   ok('Placas: se pueden renombrar TODOS los canales activos (16 campos con su id)',await up.evaluate(()=>{const l=[...document.querySelectorAll('#nmgrid label')].map(x=>x.textContent);return l.length===16&&l[0].startsWith('A1')&&l[10].startsWith('B1')&&document.querySelectorAll('#nmgrid input').length===16;}));
   ok('Placas: el campo de un canal de la 2.ª placa propone su nombre de fábrica',await up.evaluate(()=>document.getElementById('chn_12').placeholder==='C'));
   await up.click('.tab[data-tab="spk"]');await up.click('#spks .spk[data-id="0"]');await up.waitForSelector('#drawer.on');await adv(up);
   ok('Placas: el altavoz elige entre las 16 entradas y las de la 2.ª placa llevan su id',await up.evaluate(()=>{const o=[...document.querySelectorAll('#d_cl option')].map(x=>x.textContent);return o.length===16&&/^B1 FL/.test(o[10])&&/^A1 FL/.test(o[0]);}));
   ok('Placas: sin errores de JS',uerr.length===0,uerr);
   await up.close();}
  {// iPhone: sin zoom al tocar campos, sin desbordes y subpestañas que solo se mueven en horizontal
   const ip=await b.newContext({viewport:{width:393,height:852},deviceScaleFactor:3,isMobile:true,hasTouch:true});
   const mp=await ip.newPage();const merr=[];mp.on('pageerror',e=>merr.push(String(e)));
   await mp.goto(BASE+'/');await mp.waitForSelector('#spks .spk');await sleep(1200);
   ok('iPhone: campos y selectores con letra de 16 px o más (Safari no hace zoom al tocarlos)',await mp.evaluate(()=>[...document.querySelectorAll('input:not([type=range]):not([type=radio]):not([type=checkbox]),select,textarea')].every(e=>parseFloat(getComputedStyle(e).fontSize)>=16)));
   ok('iPhone: sin zoom por doble toque (touch-action: manipulation)',await mp.evaluate(()=>getComputedStyle(document.body).touchAction==='manipulation'));
   ok('iPhone: las subpestañas no se desplazan en vertical',await mp.evaluate(()=>[...document.querySelectorAll('.subtabs')].every(e=>getComputedStyle(e).overflowY==='hidden')));
   let wide=[];
   for(const tab of ['spk','snd','lat','sys']){await mp.click('.tab[data-tab="'+tab+'"]');await sleep(300);
     const subs=await mp.evaluate(t=>[...document.querySelectorAll('.subtabs button')].filter(b=>b.offsetParent).map(b=>b.id),tab);
     for(const id of (subs.length?subs:[null])){if(id){await mp.click('#'+id,{force:true}).catch(()=>{});await sleep(250);}
       const sw=await mp.evaluate(()=>document.documentElement.scrollWidth-document.documentElement.clientWidth);if(sw>1)wide.push(tab+'/'+id+':'+sw);}}
   ok('iPhone: ninguna pestaña ni subpestaña desborda en horizontal (393 px)',wide.length===0,wide);
   await mp.click('.tab[data-tab="sys"]');await mp.click('#sy_rpi');await sleep(300);
   const lw=await mp.evaluate(()=>{const l=document.getElementById('upd_log');l.hidden=false;l.textContent='[ 37%] Building CXX object CMakeFiles/sonos_wifi.dir/src/'+'ruta_larguisima_sin_espacios_'.repeat(8)+'.cpp.o\nlinea corta';return {pg:document.documentElement.scrollWidth-document.documentElement.clientWidth,card:document.getElementById('c_upd').getBoundingClientRect().right-document.documentElement.clientWidth,wrap:getComputedStyle(l).whiteSpace};});
   ok('iPhone: una línea larguísima en el registro de la actualización no ensancha la tarjeta ni la página',lw.pg<=1&&lw.card<=1&&lw.wrap==='pre-wrap',lw);
   ok('iPhone: sin errores de JS',merr.length===0,merr);
   await ip.close();}
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
  // ── Navegación de web app: barra inferior fija, riel lateral en horizontal, modales completos, Sonido por modos ──
  {const nm=await b.newContext({viewport:{width:390,height:844},deviceScaleFactor:2,isMobile:true,hasTouch:true});
   const n=await nm.newPage();const nerr=[];n.on('pageerror',e=>nerr.push(String(e)));await n.goto(BASE+'/');await n.waitForSelector('#spks .spk');await sleep(700);
   const R=(sel)=>n.evaluate(s=>{const r=document.querySelector(s).getBoundingClientRect();return {l:r.left,t:r.top,r:r.right,b:r.bottom,w:r.width,h:r.height,iw:innerWidth,ih:innerHeight};},sel);
   let t=await R('.tabs');
   ok('vertical: las pestañas son una barra fija pegada a los tres bordes inferiores (sin flotar)',Math.abs(t.l)<1&&Math.abs(t.r-t.iw)<1&&Math.abs(t.b-t.ih)<1,t);
   ok('vertical: la barra inferior tiene fondo sólido y esquinas rectas',await n.evaluate(()=>{const c=getComputedStyle(document.querySelector('.tabs'));return /^rgb\(/.test(c.backgroundColor)&&c.backdropFilter==='none'&&c.borderRadius==='0px';}));
   ok('cabecera sin degradado ni desenfoque (color sólido)',await n.evaluate(()=>{const c=getComputedStyle(document.querySelector('.top'));return /^rgb\(/.test(c.backgroundColor)&&c.backgroundImage==='none'&&(c.backdropFilter==='none'||c.backdropFilter==='');}));
   await n.tap('.tab[data-tab="snd"]');await sleep(200);await n.tap('#sb_mode');await sleep(200);
   ok('Sonido: en pass-through solo se ve la tarjeta del modo activo (sin crossover ni EQ)',await n.evaluate(()=>{const v=id=>!!document.getElementById(id).offsetParent;return v('mode_pass')&&!v('mode_proc')&&!v('c_xo')&&!v('c_eq');}));
   ok('Sonido: el selector de modo marca Pass-through (radio) y dice qué hace',await n.evaluate(()=>document.getElementById('t_pass').getAttribute('aria-checked')==='true'&&document.getElementById('t_proc').getAttribute('aria-checked')==='false'&&document.getElementById('modedesc').textContent.length>20));
   await n.tap('#t_proc');
   ok('Sonido: al elegir Procesado aparecen solo sus opciones (crossover y EQ) y se oculta pass-through',await until(async()=>await n.evaluate(()=>{const v=id=>!!document.getElementById(id).offsetParent;return v('mode_proc')&&v('c_xo')&&v('c_eq')&&!v('mode_pass');})));
   ok('Sonido: hay 3 subpestañas (Perfiles, Modo, Mezcla) y solo un panel visible a la vez',await n.evaluate(()=>document.querySelectorAll('#snd_sub .sub').length===3&&[...document.querySelectorAll('#p_snd .subpane')].filter(x=>!x.hidden).length===1));
   await n.tap('#sb_mix');
   ok('Sonido: «Mezcla» muestra modo noche y LFE, y «Modo» queda oculto',await n.evaluate(()=>!!document.getElementById('c_nt').offsetParent&&!!document.getElementById('c_lfe').offsetParent&&!document.getElementById('sp_mode').offsetParent));
   await n.focus('#sb_mix');await n.keyboard.press('ArrowRight');
   ok('subpestañas con teclado: → pasa a «Perfiles»',await n.evaluate(()=>document.activeElement.id==='sb_pf'&&document.getElementById('sb_pf').getAttribute('aria-selected')==='true'&&!document.getElementById('sp_pf').hidden));
   await n.tap('#sb_mode');
   await n.setViewportSize({width:280,height:640});await sleep(300);
   ok('subpestañas fuera de pantalla: aparece la flecha › y no la ‹',await n.evaluate(()=>{const t=document.getElementById('snd_sub');return t.scrollWidth>t.clientWidth&&document.querySelector('.sbm.r').offsetParent!==null&&document.querySelector('.sbm.l').offsetParent===null;}));
   await n.evaluate(()=>{const t=document.getElementById('snd_sub');t.scrollLeft=t.scrollWidth;});await sleep(250);
   ok('subpestañas: al llegar al final la flecha ‹ sustituye a ›',await n.evaluate(()=>document.querySelector('.sbm.l').offsetParent!==null&&document.querySelector('.sbm.r').offsetParent===null));
   await n.setViewportSize({width:390,height:844});
   await n.tap('#t_pass');await sleep(300);
   // horizontal (iPhone 16: 852×393)
   await n.setViewportSize({width:852,height:393});await sleep(400);
   t=await R('.tabs');
   ok('horizontal: las pestañas pasan a un riel fijo a la izquierda, a todo el alto',t.l<1&&t.w>=64&&t.w<=130&&Math.abs(t.h-t.ih)<2,t);
   ok('horizontal: la cabecera forma parte de la página (no queda fija) y empieza a la derecha del riel',await n.evaluate(()=>{const c=getComputedStyle(document.querySelector('.top')),r=document.querySelector('.top').getBoundingClientRect(),tb=document.querySelector('.tabs').getBoundingClientRect();return c.position==='static'&&r.left>=tb.right-1;}));
   await n.tap('.tab[data-tab="spk"]');await sleep(250);
   await n.tap('.tab[data-tab="spk"]');await sleep(250);
   const hz=await n.evaluate(()=>{const h=document.querySelector('.top').getBoundingClientRect(),tb=['#lble','#lcal','#medit'].map(x=>document.querySelector(x).getBoundingClientRect()),hh=document.getElementById('mhint').getBoundingClientRect();return {h:h.height,tb:tb.map(r=>[r.width,r.height,r.right,r.bottom]),hintTop:hh.top,iw:innerWidth,inTop:!!document.querySelector('.top #lcal')||!!document.querySelector('.top #lble')||!!document.querySelector('.top [data-theme-btn]')};});
   ok('horizontal: la cabecera cabe en una sola fila (no se amontona) y +, Calibrar y Editar plano están junto al plano con la ayuda debajo',hz.h<=70&&!hz.inTop&&hz.tb.every(r=>r[1]>=43.5&&r[0]>0&&r[2]<=hz.iw)&&hz.hintTop>=Math.max(...hz.tb.map(r=>r[3]))-1,JSON.stringify(hz));
   ok('horizontal: el riel es angosto (≤ 80 px) y el botón activo no es una píldora (radio ≤ 10 px)',await n.evaluate(()=>{const t=document.querySelector('.tabs').getBoundingClientRect(),a=getComputedStyle(document.querySelector('.tab[aria-selected=true]')).borderTopLeftRadius;return t.width<=80&&parseFloat(a)<=10}));
   ok('horizontal: en el navegador las zonas seguras laterales no se aplican (--sal/--sar = 0) y el riel pega al borde',await n.evaluate(()=>{const c=getComputedStyle(document.documentElement);return c.getPropertyValue('--sal').trim()==='0px'&&c.getPropertyValue('--sar').trim()==='0px'&&document.querySelector('.tab').getBoundingClientRect().left<=5}));
   ok('app instalada: la barra de estado es opaca (black) y sin desenfoque bajo la muesca, en las tres páginas',['/','/calibrate','/ble'].every(()=>true)&&await n.evaluate(()=>document.querySelector('meta[name=apple-mobile-web-app-status-bar-style]').content==='black')&&await n.evaluate(async()=>{for(const u of ['/calibrate','/ble']){const t=await (await fetch(u)).text();if(!/status-bar-style" content="black"/.test(t))return false}return true}));
   ok('horizontal: sin scroll horizontal y pestañas de 44 px como mínimo',await n.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1&&[...document.querySelectorAll('.tab')].every(e=>e.getBoundingClientRect().height>=44)));
   await n.tap('.tab[data-tab="spk"]');await n.tap('#spks .spk[data-id="0"]');await n.waitForSelector('#drawer.on');await adv(n);await sleep(450);
   const d=await R('#drawer'),cb=await R('#dclose');
   ok('horizontal: el modal del altavoz cabe entero en pantalla (no se corta)',d.t>=-0.5&&d.b<=d.ih+0.5&&d.r<=d.iw+0.5&&d.l>=0,d);
   ok('horizontal: el modal ocupa todo el ancho de la pantalla',Math.abs(d.w-d.iw)<1&&Math.abs(d.h-d.ih)<1,d);
   ok('horizontal: el botón de cerrar del modal es visible y táctil (≥44 px)',cb.t>=0&&cb.b<=cb.ih&&cb.h>=43.5&&cb.w>=43.5,cb);
   ok('horizontal 852 px: el contenido del modal va en rejilla de 2 columnas y la EQ ocupa la fila entera',await n.evaluate(()=>{const b=document.getElementById('dbody'),c=getComputedStyle(b).gridTemplateColumns.split(' ').length,w=[...b.children].find(x=>x.classList.contains('wide'));return getComputedStyle(b).display==='grid'&&c===2&&w&&Math.abs(w.getBoundingClientRect().width-(b.clientWidth-parseFloat(getComputedStyle(b).paddingLeft)-parseFloat(getComputedStyle(b).paddingRight)))<2;}));
   ok('horizontal: el cuerpo del modal se desplaza por dentro',await n.evaluate(()=>{const e=document.getElementById('dbody');return getComputedStyle(e).overflowY==='auto'&&e.getBoundingClientRect().bottom<=innerHeight+0.5;}));
   await n.tap('#dclose');await sleep(300);
   await n.tap('.tab[data-tab="snd"]');await sleep(200);
   await n.setViewportSize({width:932,height:430});await sleep(300);await n.tap('.tab[data-tab="lat"]');await sleep(200);
   ok('horizontal 932 px: las tarjetas de Latencia van en 3 columnas',await n.evaluate(()=>getComputedStyle(document.querySelector('#p_lat .cards')).gridTemplateColumns.split(' ').length===3));
   await n.setViewportSize({width:852,height:393});await sleep(300);await n.tap('.tab[data-tab="snd"]');await sleep(200);
   ok('horizontal: Sonido sin scroll horizontal',await n.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
   if(SHOTS){await n.screenshot({path:SHOTS+'/movil_horizontal_sonido.png'});await n.tap('.tab[data-tab="spk"]');await n.tap('#spks .spk[data-id="0"]');await n.waitForSelector('#drawer.on');await adv(n);await sleep(450);await n.screenshot({path:SHOTS+'/movil_horizontal_modal.png'});await n.tap('#dclose');await sleep(300);
     await n.setViewportSize({width:390,height:844});await sleep(300);await n.tap('.tab[data-tab="snd"]');await n.screenshot({path:SHOTS+'/movil_vertical_sonido.png'});}
   ok('navegación web app sin errores de JS',nerr.length===0,nerr);
   await nm.close();}
  await b.close();
  console.log(fails?`RESULTADO UI: ${fails} FALLOS`:'RESULTADO UI: TODO OK');process.exit(fails?1:0);
})().catch(e=>{console.error('FAIL UI excepción',e);process.exit(1);});
