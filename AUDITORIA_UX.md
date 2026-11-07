# Auditoría UX/UI (Apple HIG) y conversión a PWA

Alcance: panel `/`, `/calibrate` y `/ble` (`rpi/web/`). Método: las cinco lentes del skill apple-design (accesibilidad, layout/áreas seguras, navegación, componentes, escritura/feedback), con medidas en lugar de adjetivos. Referencias: `accessibility.md`, `layout.md`, `tab-bars.md`, `sheets.md`, `sliders.md`, `segmented-controls.md`, `feedback.md`, `writing.md`, `app-icons.md`.

**Límite honesto:** todo se verificó con Chromium (Playwright) simulando móvil 390×844. No se probó en un iPhone/Safari ni en Android reales, ni en la Raspberry real. Esto es una auditoría de HIG aplicada a una web, no una certificación de Apple.

## Resumen
La interfaz ya tenía buena base (negro + azul, español, autoguardado). Los problemas serios eran de usabilidad táctil y de accesibilidad: objetivos pequeños, texto de 14–15 px, bordes de controles casi invisibles, sin áreas seguras (el notch tapaba la cabecera) y cajones que no atrapaban el foco. Todo lo crítico está corregido y cubierto por pruebas.

## Crítico (corregido)
1. **Objetivos táctiles < 44 pt** (`accessibility.md › Controls`): botones, pestañas, selectores, campos y controles `.num` medían 28–34 px. Con `pointer:coarse` ahora tienen mínimo 44 px; el deslizador de volumen tiene pista táctil de 44 px; el interruptor amplía su zona de toque.
2. **Contraste de bordes** (`accessibility.md › Color and effects`): los bordes de controles eran casi indistinguibles del fondo. Nuevo `--ctl:#6b7a96` → 4,85:1 sobre `#000` y 4,4:1 sobre las tarjetas `#0d1016` (mínimo 3:1 para elementos no textuales). Texto: azul `#4aa3ff` 7,97:1; error `#ff6b6b` 7,57:1; ok `#3ecf8e` 10,52:1 (mínimo 4,5:1). Se añadió `prefers-contrast: more`.
3. **Áreas seguras** (`layout.md › Safe areas`): sin `viewport-fit=cover` ni `env(safe-area-inset-*)`, el notch y la barra de inicio tapaban cabecera y avisos. Ahora cabecera, laterales, barra inferior y avisos respetan los márgenes.
4. **Cajones sin gestión de foco** (`sheets.md`): ahora son `role=dialog aria-modal`, dejan inertes el panel y la cabecera, devuelven el foco al altavoz de origen y se cierran con Escape, con la X o arrastrando el asa hacia abajo en móvil.

## Mejoras
* **Tamaño de texto** (`accessibility.md › Text size`): base de 17 px (106,25 %), mínimo 11 px; la pila tipográfica empieza por `system-ui`.
* **Navegación v9.11**: la barra inferior es fija y pegada a los bordes (no flota); en móvil horizontal pasa a un riel izquierdo; modales a pantalla completa en horizontal con rejilla de 2–3 columnas; subpestañas de Sonido con indicador ‹ › de contenido fuera de pantalla; cabecera sólida sin degradado. Excepción: en horizontal los botones de la cabecera (tema, Calibrar, Añadir altavoces) son solo icono (≥ 44 px, con `aria-label` y `title`) para que quepan en una fila.
* **Navegación móvil** (`tab-bars.md`): en ≤ 760 px las cuatro secciones pasan a una barra inferior fija con icono + etiqueta (4 pestañas, dentro del máximo de 5). Pestañas con `role=tablist`, `aria-controls`, tabindex móvil y flechas/Inicio/Fin.
* **Movimiento y transparencia**: `prefers-reduced-motion` y `prefers-reduced-transparency` respetados.
* **Feedback** (`feedback.md`): los errores usan `role=alert` y duran 7 s (antes 3 s, poco para leerlos); los avisos son tocables para cerrarlos. Los LED de nivel pasan a la clase `.led` (chocaba con el control segmentado `.seg`, un bug real que ocultaba segmentos).
* **Escritura** (`writing.md`): mensajes de error con causa y acción («No se pudo guardar el procesado: …»), verbos en botones, sin «oops». Se mantiene «Tu sala» solo donde aporta contexto; el resto evita posesivos innecesarios.
* **Controles segmentados** (`segmented-controls.md`): las etiquetas largas (p. ej. «Subwoofer») hacen salto de línea en lugar de desbordar.
* **Volumen** (`sliders.md`): la HIG desaconseja deslizadores para volumen *del sistema* en iOS; aquí controlan la ganancia de un altavoz remoto, así que se mantienen, con `aria-valuetext` («−6 dB») y valor numérico visible.
* **Estado atenuado**: tarjetas desactivadas bajan a 65 % de opacidad salvo su interruptor, para que se pueda reactivar.
* **Vuelta a pass-through** visible: botón «Volver a pass-through» y «Desactivar EQ» (ya hecho antes).

## Detalles de oficio
* Icono (`app-icons.md`): forma simple, sin texto, PNG **opaco** y sin esquinas redondeadas (iOS aplica su máscara); versión *maskable* con zona segura al 80 %. `scripts/gen_icons.py` los regenera.
* `theme-color` y `background-color` `#000` para que la PWA no parpadee en blanco al arrancar.

## Lo que ya funcionaba
Modo oscuro único (justificado: se usa viendo películas en sala oscura; HIG pide respetar la preferencia del sistema, y aquí un fondo claro sería una regresión de uso), jerarquía clara, autoguardado sin botón «Guardar», estados vacíos con siguiente paso, interfaz íntegra en español.

## PWA: qué hace y qué no
* `manifest.webmanifest` (standalone, `es`, iconos any/maskable, atajos a Sonido y Latencia), `apple-touch-icon`, metaetiquetas Apple y un service worker (`/sw.js`).
* El service worker **solo se registra en contexto seguro**: HTTPS (`:8443`, certificado autofirmado) o `localhost`. En `http://IP:8080` la página funciona igual pero el navegador no ofrece «instalar».
* Estrategia: red primero con caché de respaldo solo para el armazón de las páginas; **`/api/*` nunca se cachea** (siempre datos en vivo). Sin red, abre la interfaz pero los datos no cargan.
* El nombre de caché lleva un hash del contenido: al actualizar la Raspberry, el service worker nuevo sustituye al viejo.
* Los recursos PWA se sirven **sin autenticación** (el navegador pide el manifiesto sin credenciales); no contienen datos. Las páginas y la API siguen protegidas.
* iOS: con certificado autofirmado Safari no registra el service worker salvo que el certificado esté instalado y confiado en el móvil; «Añadir a pantalla de inicio» sí da icono y modo pantalla completa. **No probado en iOS.**

## Pendiente / no verificado
* Prueba en dispositivos reales (Safari iOS, Chrome Android), VoiceOver/TalkBack y Dynamic Type del sistema (la web no lo sigue; usa su propia escala).
* Un certificado de confianza real (p. ej. CA propia instalada en los móviles) para que la instalación PWA sea completa en iOS.
