#pragma once
// UI, status rendering and WebSocket command transport only.
// Motor output, choreography, timing, calibration and Wi-Fi live in the .ino.
inline constexpr char kDroidControlPage[] = R"ROBOT_HTML(
<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#f6f9ff">
<title>Droid E3D · Play</title>
<style>
:root{color-scheme:light;--ink:#24364e;--muted:#73839b;--line:#e5ebf4;--blue:#458feb;--pink:#f1789c;--shadow:0 12px 36px #3854840b}
*{box-sizing:border-box}body{margin:0;color:var(--ink);font:15px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif;background:radial-gradient(ellipse at 5% 0%,#e5f3ff 0,transparent 40%),radial-gradient(ellipse at 100% 90%,#ffecf1 0,transparent 45%),#f7f9fe;min-height:100dvh}
button,input{font:inherit}button{cursor:pointer;-webkit-tap-highlight-color:transparent;touch-action:manipulation}button:disabled,input:disabled{opacity:.45;cursor:not-allowed}button:focus-visible,input:focus-visible,summary:focus-visible{outline:3px solid #3085e8;outline-offset:3px}button{border:0}svg{width:26px;height:26px;stroke:currentColor;stroke-width:1.8;fill:none;stroke-linecap:round;stroke-linejoin:round;flex-shrink:0}.icons{position:absolute;width:0;height:0;overflow:hidden}
h1,h2,h3,p{margin:0}.app{max-width:1380px;margin:auto;padding:20px max(20px,env(safe-area-inset-right)) 16px max(20px,env(safe-area-inset-left))}.topbar{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:18px}.brand{display:flex;align-items:center;gap:12px}.brandmark{width:47px;height:47px;border-radius:16px;background:#75b9f8;color:#fff;display:grid;place-items:center;box-shadow:0 5px 12px #70b4ee30}.brandmark svg{width:32px;height:32px}h1{font-size:21px;letter-spacing:-.7px}.brand p{font-size:12px;color:var(--muted);margin-top:1px}.toptools{display:flex;align-items:center;gap:10px}.connection{display:flex;align-items:center;gap:7px;font-size:12px;color:var(--muted);white-space:nowrap}.dot{width:8px;height:8px;border-radius:50%;background:#ed9bae;flex:none}.connection.online .dot{background:#45b998;box-shadow:0 0 0 4px #45b99815}.settings{display:flex;align-items:center;gap:8px;min-height:46px;padding:0 16px;border-radius:15px;background:#fff;color:#546986;border:1px solid var(--line);font-size:13px;font-weight:650}.settings svg{width:20px;height:20px}
.console{display:grid;grid-template-columns:minmax(260px,.9fr) minmax(0,1.35fr);gap:20px;align-items:stretch}.drive-panel{background:#fff;border:1px solid var(--line);border-radius:30px;box-shadow:var(--shadow);display:grid;place-items:center;padding:28px;min-width:0}.dpad{width:min(100%,360px);display:grid;grid-template-columns:repeat(3,minmax(0,1fr));grid-template-rows:repeat(3,1fr);gap:12px;aspect-ratio:1}.pad,.stop{display:flex;align-items:center;justify-content:center;flex-direction:column;gap:5px;border-radius:24px;min-width:0;min-height:48px;touch-action:none;user-select:none;font-size:13px;font-weight:750;transition:transform .1s,background .1s}.pad{background:#e6f3ff;color:#3788d7;border:1px solid #d2e8fc;box-shadow:0 5px 0 #d7e9f9}.pad svg{width:35px;height:35px;stroke-width:2.1}.pad.held,.pad:active{background:#cce7ff;transform:translateY(3px);box-shadow:0 2px 0 #c6def3}.pad.up{grid-area:1/2}.pad.left{grid-area:2/1}.pad.right{grid-area:2/3}.pad.down{grid-area:3/2}.stop{grid-area:2/2;background:#f67d9e;color:white;box-shadow:0 5px 0 #dc6688;font-size:12px;letter-spacing:.7px}.stop svg{width:22px;height:22px;fill:currentColor;stroke:none}.stop:active{transform:translateY(3px);box-shadow:0 2px 0 #dc6688}
.motion-panel{min-width:0;background:#fff;border:1px solid var(--line);border-radius:30px;padding:24px;box-shadow:var(--shadow)}.section-heading{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-bottom:16px}.section-heading h2{font-size:21px;letter-spacing:-.5px}.section-heading p{font-size:12px;color:var(--muted);margin-top:2px}.cancel-motion{color:#a16d89;background:#fff0f6;border:1px solid #f8e1ec;padding:9px 12px;border-radius:13px;min-height:44px;font-size:12px;font-weight:650;white-space:nowrap}.gesture-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px}.gesture{min-height:108px;border-radius:21px;padding:15px 12px;display:flex;flex-direction:column;align-items:flex-start;justify-content:center;gap:10px;text-align:left;border:1px solid transparent;color:var(--tone);background:var(--wash);transition:transform .12s,box-shadow .12s}.gesture .glyph{display:grid;place-items:center;width:36px;height:32px}.gesture strong{font-size:14px;line-height:1.25;font-weight:700}.gesture:hover{filter:brightness(.985)}.gesture:active{transform:scale(.97)}.gesture.active{border-color:var(--tone);box-shadow:inset 0 0 0 1px var(--tone)}.sky{--wash:#e9f4ff;--tone:#3c85c5}.lavender{--wash:#f0eaff;--tone:#8870c3}.mint{--wash:#e5f7ef;--tone:#3f9a7d}.peach{--wash:#fff0df;--tone:#c98940}.rose{--wash:#ffebf2;--tone:#c86c91}.lemon{--wash:#fff7d8;--tone:#b39432}.turquoise{--wash:#e2f6f7;--tone:#409a9f}.purple{--wash:#eee9ff;--tone:#8b70c8}.neutral{--wash:#edf2f8;--tone:#6b819b}
.footer{display:flex;justify-content:space-between;align-items:center;gap:12px;margin-top:14px;color:var(--muted);font-size:12px}.live-state{display:flex;align-items:center;gap:7px}.live-state:before{content:"";width:6px;height:6px;border-radius:50%;background:#a8b7cb}.live-state.moving:before{background:#a48cdb;box-shadow:0 0 0 4px #a48cdb16}.hint{font-size:11px;text-align:right}.toast{position:fixed;bottom:max(14px,env(safe-area-inset-bottom));left:50%;transform:translateX(-50%);padding:11px 17px;border-radius:14px;background:#263950;color:#fff;box-shadow:0 6px 24px #24354c25;z-index:5;font-size:13px;max-width:calc(100% - 24px);text-align:center;pointer-events:none}.toast.error{background:#b84b6a}.toast:empty{display:none}
.settings-dialog{padding:0;border:1px solid var(--line);border-radius:25px;width:min(820px,calc(100vw - 24px));max-height:calc(100dvh - 24px);color:var(--ink);box-shadow:0 24px 90px #20354c40;overflow:auto;background:#f9fbff}.settings-dialog::backdrop{background:#1e345960;backdrop-filter:blur(5px)}.modal-heading{position:sticky;top:0;z-index:2;background:#ffffffed;backdrop-filter:blur(10px);display:flex;align-items:center;justify-content:space-between;gap:10px;padding:19px 22px;border-bottom:1px solid var(--line)}.modal-heading h2{font-size:20px}.modal-heading p{font-size:12px;color:var(--muted);margin-top:3px}.close{width:44px;height:44px;flex:none;border-radius:13px;background:#edf2f8;color:#667b95;display:grid;place-items:center}.modal-content{padding:18px 22px 24px}.cal-section{background:white;border:1px solid var(--line);border-radius:19px;padding:18px;margin-bottom:14px}.cal-section:last-child{margin-bottom:0}.cal-section h3{font-size:15px;margin-bottom:13px}.subnote{font-size:12px;color:var(--muted);line-height:1.6;margin:10px 0 0}.row{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap}.value{font-weight:750;color:#438aca;font-variant-numeric:tabular-nums}input[type=range]{width:100%;height:36px;accent-color:#6aaaf0;margin:6px 0;touch-action:pan-y}input[type=number]{width:94px;min-height:44px;border:1px solid #d9e5f1;border-radius:12px;padding:8px;color:var(--ink);background:#fff}.cal-grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}.motor-block{padding:14px;background:#f8fbff;border:1px solid var(--line);border-radius:15px;min-width:0}.motor-block label{font-size:13px;font-weight:650}.small-button{min-height:44px;padding:9px 13px;border-radius:12px;background:#e8f3ff;color:#4586bd;font-size:12px;font-weight:700;border:1px solid #d8eafa}.small-button.pink{color:#bd6b8f;background:#ffedf4;border-color:#f9deea}.cal-line{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-top:10px}.cal-line label{font-size:12px;font-weight:400;flex:1}.joint-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px}.joint-card{background:#fff8fb;border:1px solid #f4e6ee;border-radius:15px;padding:14px;min-width:0}.joint-card .row{font-size:13px;font-weight:650}.joint-card input{accent-color:#e997b4}.joint-card .value{color:#c87698}.home-note{font-size:11px;color:#8b7a87;margin:2px 0 11px}.joint-card button{width:100%}.modal-note{font-size:12px;color:#8a6f81;margin-bottom:13px;min-height:17px}
@media(min-width:1100px){.app{padding-top:35px}.console{min-height:462px}.motion-panel{padding:26px}.gesture{min-height:112px}}
@media(max-width:900px){.app{padding:12px max(12px,env(safe-area-inset-right)) 12px max(12px,env(safe-area-inset-left))}.topbar{margin-bottom:12px}.console{grid-template-columns:minmax(220px,.9fr) minmax(0,1.35fr);gap:12px}.drive-panel{padding:18px;border-radius:24px}.dpad{gap:9px}.pad,.stop{border-radius:19px}.motion-panel{padding:16px;border-radius:24px}.gesture-grid{gap:9px}.gesture{min-height:88px;padding:12px 10px;gap:7px;border-radius:17px}.gesture strong{font-size:12px}.section-heading{margin-bottom:12px}.section-heading h2{font-size:18px}.footer{margin-top:10px}}
@media(orientation:landscape) and (max-height:500px){.app{padding-top:9px;padding-bottom:8px}.topbar{margin-bottom:9px}.brandmark{width:36px;height:36px;border-radius:12px}.brandmark svg{width:27px;height:27px}h1{font-size:18px}.brand p{font-size:10px}.settings{min-height:40px}.console{grid-template-columns:minmax(220px,.85fr) minmax(0,1.3fr);gap:12px}.drive-panel{padding:16px}.dpad{width:min(100%,264px);gap:8px}.pad,.stop{border-radius:18px}.pad svg{width:29px;height:29px}.pad{font-size:11px;gap:2px}.motion-panel{padding:13px 15px}.section-heading{margin-bottom:8px}.section-heading p{display:none}.section-heading h2{font-size:17px}.cancel-motion{min-height:36px;padding:7px 10px;font-size:11px}.gesture-grid{gap:8px;max-height:calc(100dvh - 155px);overflow-y:auto;overscroll-behavior:contain;scrollbar-width:thin;padding:2px}.gesture{min-height:64px;padding:9px 11px;flex-direction:row;align-items:center;justify-content:flex-start;gap:6px;border-radius:15px}.gesture .glyph{width:27px;height:27px}.gesture svg{width:24px;height:24px}.gesture strong{font-size:12px}.footer{margin-top:8px;font-size:11px}.hint{font-size:10px}.rotate-hint{display:none}.modal-heading{padding:12px 18px}.modal-content{padding:14px 18px}.settings-dialog{max-height:calc(100dvh - 16px)}}
@media(orientation:landscape) and (max-height:350px){.gesture{min-height:48px}.motion-panel{padding:10px 12px}.section-heading{margin-bottom:6px}}
@media(max-width:600px) and (orientation:portrait){.brand p{font-size:10px}.brandmark{width:38px;height:38px;border-radius:13px}.brand{gap:8px}h1{font-size:18px}.toptools{gap:7px}.settings{width:44px;padding:0;justify-content:center}.settings span{display:none}.connection{font-size:10px}.console{grid-template-columns:minmax(168px,.9fr) minmax(0,1fr);gap:10px;align-items:start}.drive-panel{padding:7px;border-radius:20px;position:sticky;top:12px}.dpad{gap:4px}.pad,.stop{border-radius:13px;font-size:10px;gap:1px;aspect-ratio:1}.pad svg{width:23px;height:23px}.stop{letter-spacing:0;font-size:10px}.stop svg{width:16px;height:16px}.motion-panel{padding:12px 10px;border-radius:20px}.section-heading{align-items:flex-start;flex-direction:column;gap:8px}.section-heading h2{font-size:16px}.section-heading p{display:none}.cancel-motion{width:100%;font-size:10px;min-height:40px;padding:6px}.gesture-grid{grid-template-columns:1fr;gap:8px}.gesture{min-height:57px;flex-direction:row;align-items:center;justify-content:flex-start;padding:9px;gap:8px;border-radius:13px}.gesture .glyph{width:24px;height:24px}.gesture svg{width:23px;height:23px}.gesture strong{font-size:12px}.footer{align-items:flex-start}.hint{max-width:145px}.joint-grid{grid-template-columns:1fr}.cal-grid{grid-template-columns:1fr}.modal-heading{padding:15px}.modal-content{padding:14px}.modal-heading h2{font-size:18px}.cal-section{padding:14px}}
@media(max-width:360px){.app{padding-left:8px;padding-right:8px}.console{grid-template-columns:156px minmax(0,1fr);gap:8px}.drive-panel{padding:6px}.pad,.stop{min-height:44px}.topbar{gap:6px}.connection{font-size:9px}.brand p{display:none}.toptools{gap:6px}.brandmark{width:31px;height:31px}.brandmark svg{width:25px;height:25px}h1{font-size:16px}.motion-panel{padding:10px 8px}.gesture{padding:8px;gap:5px}.gesture strong{font-size:11px}}
@media(prefers-reduced-motion:reduce){*{transition:none!important;scroll-behavior:auto!important}}
</style>
</head>
<body>
<svg class="icons" xmlns="http://www.w3.org/2000/svg" aria-hidden="true"><defs>
<symbol id="robot" viewBox="0 0 24 24"><rect x="4" y="6" width="16" height="13" rx="4"/><path d="M12 3v4M1 11v4m22-4v4M8 15h8"/><circle cx="8" cy="11" r=".9"/><circle cx="16" cy="11" r=".9"/></symbol>
<symbol id="up" viewBox="0 0 24 24"><path d="m5 11 7-7 7 7M12 4v16"/></symbol>
<symbol id="down" viewBox="0 0 24 24"><path d="m5 13 7 7 7-7M12 20V4"/></symbol>
<symbol id="left" viewBox="0 0 24 24"><path d="m11 5-7 7 7 7M4 12h16"/></symbol>
<symbol id="right" viewBox="0 0 24 24"><path d="m13 5 7 7-7 7M20 12H4"/></symbol>
<symbol id="stop-icon" viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16" rx="4"/></symbol>
<symbol id="settings-icon" viewBox="0 0 24 24"><path d="M4 6h16M4 12h16M4 18h16"/><circle cx="9" cy="6" r="2" fill="white"/><circle cx="15" cy="12" r="2" fill="white"/><circle cx="8" cy="18" r="2" fill="white"/></symbol>
<symbol id="close-icon" viewBox="0 0 24 24"><path d="m6 6 12 12M18 6 6 18"/></symbol>
<symbol id="look-left" viewBox="0 0 24 24"><rect x="7" y="7" width="14" height="12" rx="4"/><path d="M14 4v4M5 4 2 7l3 3M2 7h4"/><circle cx="11" cy="12" r=".8"/><circle cx="16" cy="12" r=".8"/></symbol>
<symbol id="look-right" viewBox="0 0 24 24"><rect x="3" y="7" width="14" height="12" rx="4"/><path d="M10 4v4m9-3 3 3-3 3m3-3h-4"/><circle cx="8" cy="12" r=".8"/><circle cx="13" cy="12" r=".8"/></symbol>
<symbol id="around" viewBox="0 0 24 24"><path d="M4 7a9 9 0 0 1 15 0m0-4v4h-4M20 17a9 9 0 0 1-15 0m0 4v-4h4"/><circle cx="12" cy="12" r="3"/></symbol>
<symbol id="shake" viewBox="0 0 24 24"><rect x="6" y="7" width="12" height="12" rx="4"/><path d="m3 9-2 3 2 3m18-6 2 3-2 3M12 4v4"/><circle cx="10" cy="12" r=".7"/><circle cx="14" cy="12" r=".7"/></symbol>
<symbol id="hand" viewBox="0 0 24 24"><path d="M7 12V6a1.5 1.5 0 0 1 3 0v5-7a1.5 1.5 0 0 1 3 0v7-6a1.5 1.5 0 0 1 3 0v7-4a1.5 1.5 0 0 1 3 0v7a7 7 0 0 1-12 5l-3-4a1.5 1.5 0 0 1 2-2l2 2"/></symbol>
<symbol id="sparkle" viewBox="0 0 24 24"><path d="m12 3 2.4 6.6L21 12l-6.6 2.4L12 21l-2.4-6.6L3 12l6.6-2.4L12 3ZM20 2v4m-2-2h4"/></symbol>
<symbol id="music" viewBox="0 0 24 24"><path d="M9 18V5l11-2v13M9 9l11-2"/><ellipse cx="6" cy="18" rx="3" ry="2.5"/><ellipse cx="17" cy="16" rx="3" ry="2.5"/></symbol>
<symbol id="home" viewBox="0 0 24 24"><path d="m3 11 9-8 9 8M5 10v10h14V10M9 20v-7h6v7"/></symbol>
</defs></svg>
<div class="app">
<header class="topbar">
  <div class="brand"><div class="brandmark"><svg><use href="#robot"/></svg></div><div><h1>Droid E3D</h1><p>TEST_5 · XiaoZhi · tối đa 4 chuyển động</p></div></div>
  <div class="toptools"><button id="aiToggle" class="small-button" type="button">Gọi / dừng XiaoZhi</button><a class="small-button" href="/wifi">Wi-Fi</a><div id="connection" class="connection"><span class="dot"></span><span id="connectionText">Đang kết nối</span></div><button id="openSettings" class="settings" aria-label="Mở hiệu chỉnh động cơ"><svg><use href="#settings-icon"/></svg><span>Hiệu chỉnh</span></button></div>
</header>
<main class="console">
  <section class="drive-panel" aria-label="Điều khiển di chuyển: nhấn giữ để chạy, thả để dừng">
    <div class="dpad">
      <button class="pad up" data-direction="forward" data-control aria-label="Giữ để tiến"><svg><use href="#up"/></svg><span>Tiến</span></button>
      <button class="pad left" data-direction="left" data-control aria-label="Giữ để rẽ trái"><svg><use href="#left"/></svg><span>Trái</span></button>
      <button id="stop" class="stop" data-control aria-label="Dừng tất cả chuyển động"><svg><use href="#stop-icon"/></svg><span>DỪNG</span></button>
      <button class="pad right" data-direction="right" data-control aria-label="Giữ để rẽ phải"><svg><use href="#right"/></svg><span>Phải</span></button>
      <button class="pad down" data-direction="backward" data-control aria-label="Giữ để lùi"><svg><use href="#down"/></svg><span>Lùi</span></button>
    </div>
  </section>
  <section class="motion-panel" aria-labelledby="motionTitle">
    <div class="section-heading"><div><h2 id="motionTitle">Động tác</h2><p>Cảm xúc phối hợp đầu, tay và xích.</p></div><button id="cancelGesture" class="cancel-motion" data-control>Dừng động tác</button></div>
    <div class="gesture-grid">
      <button class="gesture lemon" data-gesture="cheer" data-control aria-pressed="false"><span class="glyph"><svg><use href="#sparkle"/></svg></span><strong>Vui mừng</strong></button>
      <button class="gesture sky" data-gesture="sad" data-control aria-pressed="false"><span class="glyph"><svg><use href="#robot"/></svg></span><strong>Buồn</strong></button>
      <button class="gesture lavender" data-gesture="curious" data-control aria-pressed="false"><span class="glyph"><svg><use href="#around"/></svg></span><strong>Tò mò</strong></button>
      <button class="gesture mint" data-gesture="greet" data-control aria-pressed="false"><span class="glyph"><svg><use href="#hand"/></svg></span><strong>Chào bạn</strong></button>
      <button class="gesture purple" data-gesture="show" data-control aria-pressed="false"><span class="glyph"><svg><use href="#music"/></svg></span><strong>Biểu diễn</strong></button>
      <button class="gesture turquoise" data-gesture="shakeHead" data-control aria-pressed="false"><span class="glyph"><svg><use href="#shake"/></svg></span><strong>Lắc đầu</strong></button>
      <button class="gesture peach" data-gesture="waveLeft" data-control aria-pressed="false"><span class="glyph"><svg><use href="#hand"/></svg></span><strong>Vẫy tay trái</strong></button>
      <button class="gesture rose" data-gesture="waveRight" data-control aria-pressed="false"><span class="glyph"><svg><use href="#hand"/></svg></span><strong>Vẫy tay phải</strong></button>
      <button class="gesture mint" data-gesture="lookAround" data-control aria-pressed="false"><span class="glyph"><svg><use href="#around"/></svg></span><strong>Ngó nghiêng</strong></button>
      <button class="gesture sky" data-gesture="lookLeft" data-control aria-pressed="false"><span class="glyph"><svg><use href="#left"/></svg></span><strong>Nhìn trái</strong></button>
      <button class="gesture lavender" data-gesture="lookRight" data-control aria-pressed="false"><span class="glyph"><svg><use href="#right"/></svg></span><strong>Nhìn phải</strong></button>
      <button class="gesture neutral" data-gesture="home" data-control aria-pressed="false"><span class="glyph"><svg><use href="#home"/></svg></span><strong>Về tư thế chuẩn</strong></button>
    </div>
  </section>
</main>
<footer class="footer"><span id="motionStatus" class="live-state" role="status" aria-live="polite">Sẵn sàng kết nối</span><span class="hint">Giữ nút để chạy · Thả để dừng<span class="rotate-hint"><br>Xoay ngang điện thoại để thao tác rộng hơn.</span></span></footer>
</div>
<dialog id="settingsDialog" class="settings-dialog" aria-labelledby="settingsTitle">
  <div class="modal-heading"><div><h2 id="settingsTitle">Hiệu chỉnh robot</h2><p>Góc servo, tốc độ và điểm dừng — tất cả ở đây.</p></div><button id="closeSettings" class="close" aria-label="Đóng hiệu chỉnh"><svg><use href="#close-icon"/></svg></button></div>
  <div class="modal-content">
    <p id="modalNotice" class="modal-note" role="status" aria-live="polite">Robot dừng khi mở bảng hiệu chỉnh.</p>
    <section class="cal-section"><h3>Động tác tùy biến</h3><p id="programProgress" class="subnote">Chưa chạy chuỗi</p><div class="row"><label>Số lần vẫy <input id="waveCycles" type="number" min="1" max="20" value="3"></label><label>Biên độ (°) <input id="waveAmplitude" type="number" min="10" max="45" value="20"></label><label>Kiểu hai tay <select id="wavePattern" data-control><option value="mirror">Đối xứng</option><option value="parallel">Cùng chiều thực tế</option><option value="alternate">Luân phiên trái · phải</option></select></label><button id="waveBoth" class="small-button" data-control>Vẫy hai tay</button><button id="waveLeftCustom" class="small-button" data-control>Chỉ tay trái</button><button id="waveRightCustom" class="small-button" data-control>Chỉ tay phải</button><button id="stopProgram" class="small-button pink" data-control>Dừng chuỗi</button></div><p class="subnote">Đối xứng: hai tay đi ngược chiều thực tế. Cùng chiều: hai tay đi cùng chiều thực tế. Luân phiên: tay trái rồi tay phải. Bắt đầu với biên độ nhỏ.</p><details><summary>Chuỗi JSON nâng cao</summary><p class="subnote">Tối đa 32 bước, lặp 1–20 lần, tổng giới hạn 120 giây. Bấm kiểm tra trước khi chạy. Đóng trang hoặc mất kết nối sẽ dừng chuỗi web.</p><textarea id="motionProgram" aria-label="Chuỗi động tác JSON" rows="7" maxlength="4096" style="width:100%;margin:10px 0">{"repeat":3,"steps":[{"pose":{"leftArm":-35,"rightArm":35},"relative":true,"hold_ms":180},{"pose":{"leftArm":0,"rightArm":0},"relative":true,"hold_ms":180}]}</textarea><div class="row"><button id="checkProgram" class="small-button" data-control>Kiểm tra · không chạy</button><button id="runProgram" class="small-button pink" data-control>Chạy chuỗi</button></div></details></section>
    <section class="cal-section"><h3>Xoay theo số vòng · ước lượng</h3><p class="subnote">Chưa có encoder/IMU. Đo thời gian xoay một vòng ở tốc độ 40% trên nền thực tế, rồi nhập riêng từng chiều. Không nhập số đoán. Mỗi vòng dài hơn 5 giây được chia thành hai đoạn; cần hiệu chỉnh theo cách chạy này. Điểm dừng có thể sai khi pin hoặc mặt nền thay đổi.</p><div class="row"><label>Trái (ms) <input id="spinLeftMs" type="number" min="1000" max="10000" placeholder="Chưa đo"></label><label>Phải (ms) <input id="spinRightMs" type="number" min="1000" max="10000" placeholder="Chưa đo"></label><button id="saveSpin" class="small-button" data-control>Lưu số đã đo</button></div><div class="row" style="margin-top:12px"><label>Số vòng <input id="spinTurns" type="number" min="1" max="10" value="1"></label><button class="small-button" data-spin="left" data-control>Xoay trái</button><button class="small-button" data-spin="right" data-control>Xoay phải</button></div></section>
    <section class="cal-section"><h3>Tốc độ di chuyển</h3><label class="row" for="driveSpeed"><span>Tốc độ nút tiến / lùi / rẽ</span><span id="driveSpeedValue" class="value">45%</span></label><input id="driveSpeed" type="range" min="10" max="100" value="45" data-control></section>
    <section class="cal-section"><h3>Đầu &amp; hai tay · 180°</h3><div class="joint-grid">
      <div class="joint-card"><label class="row" for="head"><span>Đầu</span><span id="headValue" class="value">90°</span></label><input id="head" type="range" min="0" max="180" value="90" data-joint="head" data-control><p class="home-note">Mặc định: <span id="headHome">90°</span></p><button class="small-button pink" data-save-home="head" data-control>Lưu làm mặc định</button></div>
      <div class="joint-card"><label class="row" for="leftArm"><span>Tay trái</span><span id="leftArmValue" class="value">90°</span></label><input id="leftArm" type="range" min="0" max="180" value="90" data-joint="leftArm" data-control><p class="home-note">Mặc định: <span id="leftArmHome">90°</span></p><button class="small-button pink" data-save-home="leftArm" data-control>Lưu làm mặc định</button></div>
      <div class="joint-card"><label class="row" for="rightArm"><span>Tay phải</span><span id="rightArmValue" class="value">90°</span></label><input id="rightArm" type="range" min="0" max="180" value="90" data-joint="rightArm" data-control><p class="home-note">Mặc định: <span id="rightArmHome">90°</span></p><button class="small-button pink" data-save-home="rightArm" data-control>Lưu làm mặc định</button></div>
    </div><p class="subnote">Tư thế mặc định được lưu cho lần bật nguồn sau và nút “Về tư thế chuẩn”. Các động tác cũng dùng tư thế này làm mốc.</p><div class="row" style="margin-top:12px"><label>Biên độ xung đầu (µs) <input id="headSpanUs" type="number" min="400" max="700" step="25" value="500"></label><button id="saveHeadSpan" class="small-button" data-control>Lưu biên độ đầu</button></div><p class="subnote">Mặc định ±500 µs quanh 1500 µs (1000–2000). Muốn đầu xoay rộng hơn, đưa đầu về 90°, rồi tăng từng bước 25–50 µs. Tối đa ±700 µs (800–2200). Không đổi giới hạn 0–180° trong phần mềm; dừng nếu cơ cấu chạm cữ hoặc servo kêu gằn.</p></section>
    <section class="cal-section"><h3>Hai dải xích · 360°</h3><div class="cal-grid">
      <div class="motor-block"><label class="row" for="leftWheel"><span>Xích trái · GPIO4</span><span id="leftWheelValue" class="value">0</span></label><input id="leftWheel" type="range" min="-100" max="100" value="0" data-wheel data-control><div class="cal-line"><label for="leftStop">Xung dừng (µs)</label><input id="leftStop" type="number" min="1400" max="1600" step="1" value="1505" data-control></div><div class="cal-line"><button id="saveLeft" class="small-button" data-control>Áp dụng</button><button id="flipLeft" class="small-button" data-control>Đảo chiều trái</button></div></div>
      <div class="motor-block"><label class="row" for="rightWheel"><span>Xích phải · GPIO5</span><span id="rightWheelValue" class="value">0</span></label><input id="rightWheel" type="range" min="-100" max="100" value="0" data-wheel data-control><div class="cal-line"><label for="rightStop">Xung dừng (µs)</label><input id="rightStop" type="number" min="1400" max="1600" step="1" value="1500" data-control></div><div class="cal-line"><button id="saveRight" class="small-button" data-control>Áp dụng</button><button id="flipRight" class="small-button" data-control>Đảo chiều phải</button></div></div>
    </div><div class="row" style="margin-top:14px"><button id="zeroWheels" class="small-button pink" data-control>Dừng hai xích</button><span class="subnote">Âm: lùi · Dương: tiến</span></div><div class="row" style="margin-top:12px"><button class="small-button" data-track-side="left" data-track-dir="1" data-control>Trái tiến 0,25 giây</button><button class="small-button" data-track-side="left" data-track-dir="-1" data-control>Trái lùi 0,25 giây</button><button class="small-button" data-track-side="right" data-track-dir="1" data-control>Phải tiến 0,25 giây</button><button class="small-button" data-track-side="right" data-track-dir="-1" data-control>Phải lùi 0,25 giây</button></div><p class="subnote">Hai xích điều khiển độc lập bằng thanh trượt hoặc nút thử ngắn. Mỗi nút thử chỉ chạy một bên rồi tự dừng. Thanh trượt duy trì tốc độ cho đến khi về 0 hoặc bấm dừng.</p></section>
    <section class="cal-section"><h3>Màn hình 240 × 240</h3><p class="subnote">Chiều hiện tại: <b id="displayRotation">—</b>. Chọn chiều bên dưới để lưu và khởi động lại. Offset tự khớp theo chiều, không cộng hai lần.</p><div class="row"><button class="small-button" data-rotation="0">Chiều 0</button><button class="small-button" data-rotation="1">Chiều 1</button><button class="small-button" data-rotation="2">Chiều 2 · theo tài liệu</button><button class="small-button" data-rotation="3">Chiều 3</button></div></section>
    <section class="cal-section"><h3>Mic / loa · theo XIAOzITEST</h3><p class="subnote">Mic I2S0: 16 kHz, 32-bit stereo, lấy kênh trái, gain 8×, giới hạn ±30000. Loa I2S1: 16 kHz, 16-bit stereo. Giữ mức âm lượng XiaoZhi đã lưu, không tự tăng âm lượng.</p></section>
    <section class="cal-section"><h3>Touch GPIO21</h3><p class="subnote">Chạm một lần: trò chuyện, không cần giữ. Chạm đôi: dừng khẩn. Chạm ba: về home. Giữ 5 giây: cấu hình Wi-Fi. Wake word tích hợp: Hi Wall-E.</p></section>
  </div>
</dialog>
<div id="notice" class="toast" role="status" aria-live="polite"></div>
<script>
(() => {
  const $ = id => document.getElementById(id);
  const pads = [...document.querySelectorAll('[data-direction]')];
  const controls = [...document.querySelectorAll('[data-control]')];
  const gestures = [...document.querySelectorAll('[data-gesture]')];
  const joints = ['head','leftArm','rightArm'];
  const gestureNames = {lookLeft:'Nhìn trái',lookRight:'Nhìn phải',lookAround:'Ngó nghiêng',shakeHead:'Lắc đầu',waveLeft:'Vẫy tay trái',waveRight:'Vẫy tay phải',cheer:'Vui mừng',sad:'Buồn',curious:'Tò mò',greet:'Chào bạn',show:'Biểu diễn',home:'Về tư thế chuẩn'};
  const jointNames = {head:'đầu',leftArm:'tay trái',rightArm:'tay phải'};
  const dirtyNeutral = new Set(), pendingJoint = {};
  let ws, owner=false, pwmReady=false, mode='', direction='', timer=null, activePad=null;
  let heartbeat=null, activeGesture='idle', toastTimer=null;
  const dialog=$('settingsDialog');
  const notice=(text,error=false) => {
    $('notice').textContent=text; $('notice').classList.toggle('error',error);
    $('modalNotice').textContent=text;
    clearTimeout(toastTimer); toastTimer=setTimeout(()=>$('notice').textContent='',3800);
  };
  const setStatus=(text,online) => {
    $('connectionText').textContent=text; $('connection').classList.toggle('online',online);
    controls.forEach(e=>e.disabled=!(owner&&pwmReady));
    $('cancelGesture').disabled=!(owner&&pwmReady&&activeGesture!=='idle');
  };
  let requestBusy=false, queue=[], polling=false;
  const packet=text=>{
    const p=text.split('|');
    switch(p[0]){
      case 'STOP': return {command:'stop_tracks'};
      case 'HALT': case 'CANCEL': return {command:'stop'};
      case 'PING': return {command:'heartbeat'};
      case 'DRIVE': return {command:'drive',direction:p[1],speed:Number(p[2])};
      case 'WHEELS': return {command:'wheels',left:Number(p[1]),right:Number(p[2])};
      case 'GESTURE': return {command:'gesture',name:p[1]};
      case 'ANGLE': return {command:'joint',joint:p[1],angle:Number(p[2]),speed:100};
      case 'HOMESET': return {command:'save_home',joint:p[1],angle:Number(p[2])};
      case 'CAL': return {command:'neutral',side:p[1],pulse:Number(p[2])};
      case 'FLIP': return {command:'flip',side:p[1]};
      case 'ROTATION': return {command:'display_rotation',rotation:Number(p[1])};
      default: return null;
    }
  };
  const renderJson=s=>{
    const t=s.tracks,j=s.joints;
    if(!t||!j)return;
    if(document.activeElement!==$('headSpanUs'))$('headSpanUs').value=String(j.head.spanUs??500);
    $('displayRotation').textContent=String(s.displayRotation??2);
    const p=s.program||{};
    const names={idle:'Chưa chạy',running:'Đang chạy',completed:'Đã chạy hết lệnh',stopped:'Đã dừng',timeout:'Hết thời gian an toàn',heartbeat_timeout:'Dừng do mất kết nối',pwm_error:'Lỗi PWM'};
    $('programProgress').textContent=(names[p.status]||p.status||'Chưa chạy')+' · hoàn tất '+(p.cyclesDone||0)+'/'+(p.repeat||1)+' lượt'+(p.frameCount?' · bước '+(p.frame+1)+'/'+p.frameCount:'');
    if(s.spinCalibration)['left','right'].forEach(side=>{const e=$('spin'+(side==='left'?'Left':'Right')+'Ms');if(document.activeElement!==e&&!e.dataset.dirty)e.value=s.spinCalibration[side+'Ms']||'';});
    renderState(['STATE',t.left,t.right,j.head.target,j.leftArm.target,j.rightArm.target,
      t.leftStopUs,t.rightStopUs,t.leftSign,t.rightSign,s.pwmReady?1:0,
      j.head.home,j.leftArm.home,j.rightArm.home,s.gesture].map(String));
  };
  const pump=async()=>{
    if(requestBusy||!queue.length)return;
    const item=queue.shift();
    if(item.expires&&performance.now()>item.expires){pump();return;}
    requestBusy=true;
    try{
      const r=await fetch('/api/robot',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify(item.body),signal:AbortSignal.timeout(1500)});
      const reply=await r.json();
      if(!r.ok||!reply.success)throw new Error(reply.error||'Lệnh bị từ chối');
      if(item.body.command==='neutral'){dirtyNeutral.delete(item.body.side);notice('Đã lưu xung dừng.');}
      if(item.body.command==='save_home')notice('Đã lưu góc mặc định.');
      if(item.body.command==='flip')notice('Đã lưu chiều quay.');
      if(item.body.command==='display_rotation')notice(reply.message);
      if(['sequence','wave_arms','spin_turns','spin_calibration','track_test','head_span'].includes(item.body.command))notice(reply.message);
      if(item.body.command==='spin_calibration'){$('spinLeftMs').dataset.dirty='';$('spinRightMs').dataset.dirty='';}
      if(reply.state)renderJson(reply.state);
    }catch(e){
      queue=[];
      Object.keys(pendingJoint).forEach(k=>delete pendingJoint[k]);
      clearMotion(false);
      notice(e.message||'Mất kết nối · robot tự dừng',true);
    }finally{requestBusy=false;pump();}
  };
  const send=text=>{
    if(!owner||!pwmReady)return false;
    const body=typeof text==='string'?packet(text):text;if(!body)return false;
    const stop=body.command==='stop'||body.command==='stop_tracks';
    if(stop)queue=[]; // Never replay a queued movement after releasing STOP.
    const key=body.command==='joint'?'joint:'+body.joint:body.command;
    const coalesce=['joint','drive','wheels','heartbeat'].includes(body.command);
    if(coalesce)queue=queue.filter(item=>item.key!==key);
    if(queue.length>=16)return false;
    const transient=['joint','drive','wheels','heartbeat','gesture','sequence','wave_arms','spin_turns'].includes(body.command);
    queue.push({body,key,expires:transient?performance.now()+400:0});
    pump();return true;
  };
  const refreshWheels=()=>['left','right'].forEach(side=>$(side+'WheelValue').textContent=$(side+'Wheel').value);
  const clearMotion=(tell=true)=>{
    if(timer!==null)clearInterval(timer);
    timer=null;mode='';direction='';activePad=null;
    pads.forEach(p=>p.classList.remove('held'));
    $('leftWheel').value='0';$('rightWheel').value='0';refreshWheels();
    if(tell)send('STOP');
  };
  const halt=()=>{clearMotion(false);Object.keys(pendingJoint).forEach(k=>delete pendingJoint[k]);send('HALT');};
  const tick=()=>{
    if(mode==='pad')send('DRIVE|'+direction+'|'+$('driveSpeed').value);
    else if(mode==='wheels')send('WHEELS|'+$('leftWheel').value+'|'+$('rightWheel').value);
  };
  const beginTicker=()=>{if(timer!==null)clearInterval(timer);tick();timer=setInterval(tick,180);};
  const startHeartbeat=()=>{
    if(heartbeat!==null)clearInterval(heartbeat);
    heartbeat=setInterval(()=>{if(!document.hidden)send('PING');},250);
  };
  const renderState=items=>{
    pwmReady=items[10]==='1';activeGesture=items[14];
    if(!pwmReady){clearMotion(false);notice('Không khởi tạo được servo. Xem Serial Monitor 115200.',true);}
    setStatus(!pwmReady?'Lỗi servo':owner?'Đã kết nối':'Chế độ xem',owner&&pwmReady);
    ['left','right'].forEach((side,i)=>{if(!dirtyNeutral.has(side))$(side+'Stop').value=items[6+i];});
    $('flipLeft').textContent='Đảo trái ('+(items[8]==='-1'?'−':'+')+')';
    $('flipRight').textContent='Đảo phải ('+(items[9]==='-1'?'−':'+')+')';
    joints.forEach((joint,i)=>{
      const target=items[3+i];
      if(pendingJoint[joint]===target)delete pendingJoint[joint];
      if(pendingJoint[joint]===undefined&&document.activeElement!==$(joint)){
        $(joint).value=target;$(joint+'Value').textContent=target+'°';
      }
      $(joint+'Home').textContent=items[11+i]+'°';
    });
    if(mode===''){$('leftWheel').value=items[1];$('rightWheel').value=items[2];refreshWheels();}
    gestures.forEach(button=>{const on=button.dataset.gesture===activeGesture;button.classList.toggle('active',on);button.setAttribute('aria-pressed',String(on));});
    const moving=items[1]!=='0'||items[2]!=='0';
    $('motionStatus').textContent=activeGesture!=='idle'?'Đang '+(gestureNames[activeGesture]||'chuyển động').toLowerCase():(moving?'Đang di chuyển':'Sẵn sàng chơi cùng bạn');
    $('motionStatus').classList.toggle('moving',moving||activeGesture!=='idle');
  };
  const connect=async()=>{
    if(polling)return;
    polling=true;
    try{
      const r=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(1500)});
      if(!r.ok)throw new Error('status');
      const s=await r.json();owner=true;renderJson(s);
      if(heartbeat===null)startHeartbeat();
    }catch(e){
      owner=false;pwmReady=false;queue=[];clearMotion(false);
      if(heartbeat!==null)clearInterval(heartbeat);heartbeat=null;
      setStatus('Đang kết nối lại',false);
      $('motionStatus').textContent='Mất kết nối · robot sẽ tự dừng';
    }finally{polling=false;setTimeout(connect,650);}
  };
  controls.forEach(e=>e.disabled=true);
  pads.forEach(pad=>{
    const begin=()=>{clearMotion();activePad=pad;mode='pad';direction=pad.dataset.direction;pad.classList.add('held');beginTicker();};
    pad.addEventListener('pointerdown',event=>{if(!owner||!pwmReady)return;event.preventDefault();begin();pad.setPointerCapture(event.pointerId);});
    ['pointerup','pointercancel','lostpointercapture'].forEach(name=>pad.addEventListener(name,()=>{if(activePad===pad)clearMotion();}));
    pad.addEventListener('keydown',event=>{if(!owner||!pwmReady||event.repeat||![' ','Enter'].includes(event.key))return;event.preventDefault();begin();});
    pad.addEventListener('keyup',event=>{if([' ','Enter'].includes(event.key)&&activePad===pad)clearMotion();});
    pad.addEventListener('blur',()=>{if(activePad===pad)clearMotion();});
  });
  $('stop').addEventListener('click',halt);
  $('aiToggle').addEventListener('click',async()=>{try{const r=await fetch('/api/robot',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:'ai_toggle'}),signal:AbortSignal.timeout(2000)});const reply=await r.json();notice(reply.message||reply.error||'Lệnh XiaoZhi đã gửi',!r.ok);}catch(e){notice('Không gửi được lệnh XiaoZhi. Kiểm tra kết nối.',true);}});
  gestures.forEach(button=>button.addEventListener('click',()=>{
    clearMotion(false);
    Object.keys(pendingJoint).forEach(key=>delete pendingJoint[key]);
    send('GESTURE|'+button.dataset.gesture);
  }));
  $('cancelGesture').addEventListener('click',()=>send('CANCEL'));
  $('openSettings').addEventListener('click',()=>{halt();$('modalNotice').textContent='Robot dừng khi mở bảng hiệu chỉnh.';dialog.showModal();});
  const closeSettings=()=>{halt();dialog.close();};
  $('closeSettings').addEventListener('click',closeSettings);
  dialog.addEventListener('cancel',event=>{event.preventDefault();closeSettings();});
  // A backdrop click is outside the dialog's own rectangle.
  dialog.addEventListener('click',event=>{if(event.target!==dialog)return;const r=dialog.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)closeSettings();});
  $('driveSpeed').addEventListener('input',()=>{$('driveSpeedValue').textContent=$('driveSpeed').value+'%';if(mode==='pad')tick();});
  joints.forEach(joint=>$(joint).addEventListener('input',()=>{const value=$(joint).value;$(joint+'Value').textContent=value+'°';if(send('ANGLE|'+joint+'|'+value))pendingJoint[joint]=value;}));
  document.querySelectorAll('[data-save-home]').forEach(button=>button.addEventListener('click',()=>{
    const joint=button.dataset.saveHome;const value=$(joint).value;
    // STOP affects tracks only; the joint can finish reaching the angle being saved.
    clearMotion();send('HOMESET|'+joint+'|'+value);
  }));
  document.querySelectorAll('[data-wheel]').forEach(slider=>slider.addEventListener('input',()=>{
    if(!owner||!pwmReady)return;activePad=null;pads.forEach(p=>p.classList.remove('held'));
    mode='wheels';direction='';refreshWheels();
    if(Number($('leftWheel').value)===0&&Number($('rightWheel').value)===0)clearMotion();else beginTicker();
  }));
  $('zeroWheels').addEventListener('click',()=>clearMotion());
  ['left','right'].forEach(side=>{
    const title=side==='left'?'Left':'Right';
    $(side+'Stop').addEventListener('input',()=>dirtyNeutral.add(side));
    $('save'+title).addEventListener('click',()=>{
      const raw=$(side+'Stop').value,value=Number(raw);
      if(raw.trim()===''||!Number.isInteger(value)||value<1400||value>1600){notice('Xung dừng phải từ 1400 đến 1600 µs.',true);return;}
      clearMotion();if(send('CAL|'+side+'|'+value))notice('Đang lưu hiệu chỉnh…');
    });
    $('flip'+title).addEventListener('click',()=>{clearMotion();send('FLIP|'+side);});
  });
  window.addEventListener('blur',halt);
  window.addEventListener('pagehide',halt);
  document.addEventListener('visibilitychange',()=>{if(document.hidden)halt();});
  window.addEventListener('beforeunload',halt);
  document.querySelectorAll('[data-rotation]').forEach(button=>button.addEventListener('click',()=>{
    if(confirm('Lưu chiều màn hình và khởi động lại robot?')){halt();send('ROTATION|'+button.dataset.rotation);}
  }));
  $('saveHeadSpan').onclick=()=>{const span=Number($('headSpanUs').value);if(!Number.isInteger(span)||span<400||span>700||span%25){notice('Nhập biên độ 400–700 µs, bước 25.',true);return;}if(confirm('Đầu đang ở 90° và robot đứng yên? Tăng dần, không ép servo chạm cữ.'))send({command:'head_span',span_us:span});};
  const wave=arms=>send({command:'wave_arms',arms,cycles:Number($('waveCycles').value),amplitude:Number($('waveAmplitude').value),pattern:$('wavePattern').value});
  $('waveBoth').onclick=()=>wave('both');
  $('waveLeftCustom').onclick=()=>wave('left');
  $('waveRightCustom').onclick=()=>wave('right');
  document.querySelectorAll('[data-track-side]').forEach(button=>button.onclick=()=>{halt();send({command:'track_test',side:button.dataset.trackSide,direction:Number(button.dataset.trackDir)});});
  $('stopProgram').onclick=halt;
  $('checkProgram').onclick=()=>send({command:'sequence',program:$('motionProgram').value,dry_run:true});
  $('runProgram').onclick=()=>{if(confirm('Chạy chuỗi này? Đặt robot ở khoảng trống an toàn.'))send({command:'sequence',program:$('motionProgram').value,dry_run:false});};
  ['spinLeftMs','spinRightMs'].forEach(id=>$(id).oninput=()=>$(id).dataset.dirty='1');
  $('saveSpin').onclick=()=>{if(confirm('Lưu thời gian một vòng bạn đã đo ở tốc độ 40%?'))send({command:'spin_calibration',left_ms:Number($('spinLeftMs').value),right_ms:Number($('spinRightMs').value)});};
  document.querySelectorAll('[data-spin]').forEach(b=>b.onclick=()=>{if(confirm('Xoay ước lượng '+$('spinTurns').value+' vòng? Dọn khoảng trống và giữ nút DỪNG trong tầm tay.'))send({command:'spin_turns',direction:b.dataset.spin,turns:Number($('spinTurns').value)});});
  connect();
})();
</script>
</body>
</html>
)ROBOT_HTML";

inline constexpr char kDroidWifiPage[] = R"DROID_HTML(
<!doctype html><html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name="theme-color" content="#f6f9ff"><title>Droid E3D Wi-Fi</title><style>
:root{color-scheme:light;--ink:#24364e;--muted:#73839b;--line:#e5ebf4;--blue:#458feb;--pink:#f1789c;--green:#45b998}*{box-sizing:border-box}body{margin:0;min-height:100dvh;background:radial-gradient(ellipse at 5% 0%,#e5f3ff 0,transparent 40%),radial-gradient(ellipse at 100% 90%,#ffecf1 0,transparent 45%),#f7f9fe;color:var(--ink);font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}.shell{width:min(760px,100%);margin:auto;padding:18px max(14px,env(safe-area-inset-right)) 32px max(14px,env(safe-area-inset-left))}.top{display:flex;align-items:center;justify-content:space-between;gap:14px;margin-bottom:18px}.brand{display:flex;align-items:center;gap:11px}.face{width:48px;height:48px;border-radius:16px;background:#75b9f8;color:#fff;display:grid;place-items:center;font-size:25px;box-shadow:0 5px 12px #70b4ee30}.brand h1{font-size:21px;margin:0}.brand p{margin:2px 0 0;color:var(--muted);font-size:12px}.nav{display:flex;gap:7px;background:#fff;border:1px solid var(--line);padding:5px;border-radius:14px}.nav a{color:var(--muted);text-decoration:none;padding:8px 12px;border-radius:10px;font-size:13px;font-weight:700}.nav a.active{background:#e8f3ff;color:#4586bd}.card{background:#fff;border:1px solid var(--line);border-radius:22px;padding:20px;margin-bottom:13px;box-shadow:0 12px 36px #3854840b}.card h2{font-size:18px;margin:0 0 13px}.status{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}.item{background:#f8fbff;border:1px solid var(--line);border-radius:14px;padding:13px}.item span{display:block;color:var(--muted);font-size:12px}.item strong{display:block;margin-top:3px;overflow-wrap:anywhere}.ok{color:var(--green)}button{width:100%;border:0;border-radius:13px;padding:13px 16px;background:#f67d9e;color:#fff;font:800 14px system-ui;cursor:pointer}button:disabled{opacity:.5}.note{color:var(--muted);font-size:13px;margin:10px 0 0}.steps{margin:0;padding-left:20px;color:var(--muted)}.steps li{margin:8px 0}.wake,#message{color:var(--blue)}#message{min-height:22px;margin-top:10px}@media(max-width:560px){.brand p{display:none}.nav a{padding:7px 9px}.status{grid-template-columns:1fr}}</style></head><body><div class="shell">
<header class="top"><div class="brand"><div class="face">🤖</div><div><h1>Droid E3D</h1><p>Kết nối XiaoZhi</p></div></div><nav class="nav"><a href="/control">Điều khiển</a><a class="active" href="/wifi">Wi-Fi</a></nav></header>
<section class="card"><h2>Trạng thái mạng</h2><div class="status"><div class="item"><span>Internet (STA)</span><strong id="connected">Đang đọc</strong></div><div class="item"><span>Wi-Fi Internet</span><strong id="ssid">—</strong></div><div class="item"><span>IP trên router</span><strong id="ip">—</strong></div><div class="item"><span>Tín hiệu</span><strong id="rssi">—</strong></div><div class="item"><span>AP điều khiển</span><strong class="ok">DroidE3D-Control</strong></div><div class="item"><span>Trang điều khiển</span><strong>192.168.4.1:8080</strong></div></div><p class="note">AP điều khiển chạy song song với kết nối Internet. Kết nối điện thoại vào <b>DroidE3D-Control</b> rồi mở <b>http://192.168.4.1:8080/control</b>.</p></section>
<section class="card"><h2>Cấu hình lại Wi-Fi Internet</h2><p class="note">Robot sẽ tạm dừng XiaoZhi và AP điều khiển, sau đó mở portal cấu hình chuẩn ở cổng 80.</p><button id="reconfigure">Mở chế độ cấu hình Wi-Fi</button><div id="message"></div><ol class="steps"><li>Kết nối điện thoại vào Wi-Fi có tên bắt đầu bằng <b>Xiaozhi</b>.</li><li>Mở <b>http://192.168.4.1</b>, chọn Wi-Fi Internet và nhập mật khẩu.</li><li>Khi robot kết nối Internet lại, AP <b>DroidE3D-Control</b> tự bật trở lại.</li></ol></section>
<section class="card"><h2>Nút touch và wake word</h2><p class="note">Chạm một lần GPIO21 để bắt đầu trò chuyện, không cần giữ. Giữ đủ <b>5 giây</b> để cấu hình lại Wi-Fi.</p><p class="note">Model tích hợp nhận câu <b class="wake">Hi Wall-E</b>. Để nhận chính xác âm “Hey Wall-E” cần model WakeNet huấn luyện riêng.</p></section>
</div><script>(()=>{const $=s=>document.querySelector(s);async function load(){try{const r=await fetch('/api/status',{cache:'no-store'}),s=await r.json(),n=s.network||{};$('#connected').textContent=n.connected?'Đã kết nối':'Chưa kết nối';$('#connected').classList.toggle('ok',!!n.connected);$('#ssid').textContent=n.ssid||'—';$('#ip').textContent=n.ip||'—';$('#rssi').textContent=n.connected?n.rssi+' dBm':'—'}catch(e){$('#connected').textContent='Mất kết nối'}}$('#reconfigure').onclick=async()=>{if(!confirm('Dừng kết nối hiện tại và mở portal cấu hình Wi-Fi?'))return;$('#reconfigure').disabled=true;$('#message').textContent='Đang chuyển chế độ. Hãy kết nối vào AP Xiaozhi…';try{await fetch('/api/wifi/reconfigure',{method:'POST'})}catch(e){}};load();setInterval(load,2000)})();</script></body></html>
)DROID_HTML";
