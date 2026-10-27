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
  // Móvil
  const m=await b.newContext({viewport:{width:390,height:844},deviceScaleFactor:2,isMobile:true,hasTouch:true});
  const q=await m.newPage();await q.goto(BASE+'/');await q.waitForSelector('#spks .spk');await sleep(800);
  ok('móvil: sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_altavoces.png'});
  await q.tap('#spks .spk[data-id="0"]');await q.waitForSelector('#drawer.on');await sleep(400);
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_cajon.png'});
  await q.tap('#dclose');await q.click('.tab[data-tab="snd"]');await sleep(300);
  ok('móvil: Sonido sin scroll horizontal',await q.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  if(SHOTS)await q.screenshot({path:SHOTS+'/movil_sonido.png',fullPage:true});
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
