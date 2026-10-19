fetch("/cgi-bin/get_5g_info").then(function(r){return r.json();}).then(function(d){window._aiqosProfile=(d&&d.profile)||{};}).catch(function(){});
function loadTheme(){var t='dark';try{t=localStorage.getItem('cpe_theme')||'dark'}catch(e){}if(t==='light')document.body.classList.remove('dark');else document.body.classList.add('dark')} function toggleTheme(){var dark=document.body.classList.toggle('dark');try{localStorage.setItem('cpe_theme',dark?'dark':'light')}catch(e){}} loadTheme();
function esc(s){return String(s).replace(/[&<>"']/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c];});}
function H(v){return (v===null||v===undefined||v===''||v==='undefined'||v==='null'||v==='NaN'||v==='????')?'--':esc(v)}
function num(v,fb){var n=parseFloat(v);return isNaN(n)?(fb||0):n}
// v79: 速率单位 Mbps → MB/s（÷8）；带宽走 fmtBw/aiqosBw 不受影响
function split(v){var n=(num(v,0)/8).toFixed(2),p=n.split('.');return {i:p[0],d:'.'+p[1]}}
function barGrad(p){if(p>=70)return ['#4ade80','#15803d'];if(p>=40)return ['#facc15','#b45309'];return ['#f87171','#b91c1c']}
function iconGrad(a,b){return '--grad-a:'+a+';--grad-b:'+b}
function antColor(v){var n=num(v,NaN);if(isNaN(n))return 'var(--text2)';var p=Math.max(0,Math.min(100,((n+130)/70)*100));if(p>=70)return '#4ade80';if(p>=40)return '#facc15';return '#f87171'}
function antPct(v){var n=num(v,NaN);if(isNaN(n))return 0;return Math.max(0,Math.min(100,((n+130)/70)*100))}
var _ema={rsrp:null,rsrq:null,sinr:null};var EMA_A=0.25;
// BUGFIX 2026-09-10 23:5x (v63.4 RSRQ 永久自锁 "--"): 上一版只处理"首次 null 缓存返 NaN",
// 但未处理"缓存或新值已经是 NaN 后的 EMA 污染"——_ema[key]+0.25*(NaN-_ema[key]) 永远 = NaN → 自锁 "--" 永不自愈。
// 新版: NaN 输入时保留上次有效数字 (stale 保持, 对齐 collector lastValid* 机制);
// 缓存若已是 NaN 视为空, 下次有效输入可重置。
function ema(key,newVal){
  var vEmpty=(v)=>v==null||isNaN(v);
  if(vEmpty(newVal))return vEmpty(_ema[key])?NaN:_ema[key];
  if(vEmpty(_ema[key])){_ema[key]=newVal;return newVal}
  _ema[key]=_ema[key]+EMA_A*(newVal-_ema[key]);return _ema[key]}
// BUGFIX 2026-08-28 15:10: 面板同向天线(全部朝向一处)寻星——directional 的 az/el
// 相位差偏移角不可信(4路极差天然 1~3dB,角度抖动±8°)。改用"旋转试错法":
// 记录 RSRP 时间序列,比较近期趋势给方向引导(信号变强→保持,变弱→回转)。
var _sigHist={t:[],rsrp:[]};var SIG_WINDOW=12;var SIG_MIN_SAMPLES=4;
function sigPush(rsrp){
  if(rsrp===null||rsrp===undefined||isNaN(rsrp)||rsrp>0)return;
  var now=Date.now();
  _sigHist.t.push(now);_sigHist.rsrp.push(rsrp);
  while(_sigHist.t.length>SIG_WINDOW){_sigHist.t.shift();_sigHist.rsrp.shift();}
}
// 返回近期信号趋势: +1 上升, -1 下降, 0 平稳/数据不足
function sigTrend(){
  var n=_sigHist.rsrp.length;
  if(n<SIG_MIN_SAMPLES)return 0;
  var span=_sigHist.t[n-1]-_sigHist.t[0];
  if(span<15000)return 0; // 观察窗口 <15s 判定不足
  var d=_sigHist.rsrp[n-1]-_sigHist.rsrp[0];
  if(Math.abs(d)<1.5)return 0; // 变化 <1.5dB 视为平稳
  return d>0?1:-1;
}
function sigDelta(){var n=_sigHist.rsrp.length;return n>=2?(_sigHist.rsrp[n-1]-_sigHist.rsrp[0]):0}
var PLMN_MAP={'46000':'中国移动','46002':'中国移动','46007':'中国移动','46008':'中国移动','46001':'中国联通','46006':'中国联通','46009':'中国联通','46003':'中国电信','46005':'中国电信','46011':'中国电信','46015':'中国广电','46004':'中国广电','46048':'中国广电','46049':'中国电信','46099':'中国电信'};
function plmn2op(p){return PLMN_MAP[p]||''}
function fmtBw(v){var s=H(v);if(s==='--')return s;s=s.replace(/M\s*$/i,'').replace(/\s*MHz$/i,'').trim();return s+' MHz'}
function fmtTemp(v){var s=H(v);if(s==='--')return s;if(s.indexOf('\u00b0C')<0)s+=' \u00b0C';return s}

var _lastData=null;
function keepLast(next){
  var prev=_lastData||{};
  var out={};
  for(var k in prev)out[k]=prev[k];
  for(var k in next){
    var v=next[k];
    if(v===null||v===undefined||v===''||v==='--'||v==='N/A'||v==='undefined'||v==='null'){
      if(out[k]===undefined)out[k]=v;
    }else out[k]=v;
  }
  return out;
}
var _aiqosStatus=null;
var _aiqosMode='auto';
var _lastBwShown='';        // v63.5 hotfix (2026-09-11): 带宽显示 stale 保持——引擎重启窗口不再闪 '--'
var _lastValidAiqosMode=''; // v63.5 hotfix: 天线模式 stale 保持——接口 mode 空时不再回落 auto 造成按钮/提示跳变
var _aiqosSpatial=null;
var _aiqosLoading=false;
var _aiqosModeLoaded=false;
var _directionalEvenCount=0;
var _failsafeMsg='';
var _userModeAttempts=0;
var _userModeLast='';
var _pendingForceMode='';
var _forcedAntennaMode='';

function antennaGuidance(d){
  var mode=_aiqosMode||'auto';
  var raw=[num(d.antMain,NaN),num(d.antDiv,NaN),num(d.antMimo1,NaN),num(d.antMimo2,NaN)];
  var clean=[];
  for(var i=0;i<raw.length;i++){if(!isNaN(raw[i])&&raw[i]>-150&&raw[i]<0)clean.push({v:raw[i],i:i});}
  if(!clean.length)return {score:'--',advice:'等待天线数据',spread:0,max:0,idx:-1,rank:'--',deltaV:0,deltaH:0,raw:raw};
  var min=clean[0].v,max=clean[0].v,idx=clean[0].i;
  for(var j=1;j<clean.length;j++){if(clean[j].v>max){max=clean[j].v;idx=clean[j].i;}if(clean[j].v<min)min=clean[j].v;}
  var spread=max-min;
  var sinr=num(d.sinr,NaN);
  // BUGFIX 2026-09-09: Rank 因子乘入 SRQI——空口吞吐能力 = 信号质量 × 空间流数。
  // Rank1 单流最多 40 分, Rank2 65, Rank3 85, Rank4 才可能满分 (修复 Rank1 显示 100 分的失真)。
  var rank=spread<3.5?4:spread<6?3:spread<10?2:1;
  var rankFactor=rank>=4?1.0:rank===3?0.85:rank===2?0.65:0.40;
  var score=Math.max(0,Math.min(100,((max+130)/70)*55+(isNaN(sinr)?0:Math.max(0,Math.min(45,sinr*3)))));
  if(!isNaN(sinr)&&sinr<5)score=Math.min(score,39+sinr*2);
  score=Math.round(score*rankFactor);
  // BUGFIX 2026-08-28 15:00: 修复 clean[] 下标与物理天线错位 + 6dB 阈值对定向天线永不触发。
  // - 用 raw[0..3] 直接映射物理天线(antMain=Ant0,antDiv=Ant1,antMimo1=Ant2,antMimo2=Ant3),
  //   不再依赖过滤后 clean[] 的排列顺序(某路无效时 clean[0]≠Ant0,导致垂直/水平判错)。
  // - 阈值与天线模式绑定: 定向天线(directional) 4路天然接近(极差 1~3dB),
  //   阈值降到 1.5dB 才能触发微调建议; 全向(omni) 保持 6dB 防误报; auto 取中间 3dB。
  var a0=raw[0],a1=raw[1],a2=raw[2],a3=raw[3];
  var hasV=!isNaN(a0)&&!isNaN(a1);
  var hasH=!isNaN(a2)&&!isNaN(a3);
  var deltaV=hasV?Math.abs(a0-a1):0;
  var deltaH=hasH?Math.abs(a2-a3):0;
  var thV=mode==='directional'?1.5:(mode==='omni'?6:3);
  var thH=mode==='directional'?1.5:(mode==='omni'?6:3);
  var advice;
  if(mode==='directional'){
    // BUGFIX 2026-08-28 15:10: 面板同向天线(4单元全部朝向一处)——directional 相位差
    // 偏移角不可信(4路极差天然1~3dB,角度抖动±8°)。改用"旋转试错法"趋势引导:
    // 信号在变强→保持方向继续微调;变弱→回转;平稳→任选方向转15°试。
    var tr=sigTrend();
    var curRsrp=max;
    sigPush(curRsrp);
    if(tr>0)advice='信号正变强(+'+sigDelta().toFixed(1)+'dB)，保持当前旋转方向继续微调';
    else if(tr<0)advice='信号在变弱('+sigDelta().toFixed(1)+'dB)，请向反方向回转 10~15°';
    else if(spread<3.5)advice='四路均衡、信号平稳('+curRsrp.toFixed(1)+'dBm)，试向任一方向转15°观察信号变化';
    else advice='Rx'+(idx+1)+' 最强 '+max.toFixed(1)+' dBm，可向该侧旋转 CPE 对准基站';
  }else{
    if(spread<3.5)advice='四路天线均衡，适合全向模式；若需指向请检查 MIMO 空间流';
    else if(!isNaN(sinr)&&sinr<3&&max>-80)advice='强信号低 SINR，疑似同频反射，建议移动 CPE 位置';
    else if(hasV&&deltaV>6)advice='垂直极化偏弱(Δ'+deltaV.toFixed(1)+'dB)，建议向上微调仰角';
    else if(hasH&&deltaH>6)advice='水平极化偏弱(Δ'+deltaH.toFixed(1)+'dB)，建议左右微调方位';
    else advice='Rx'+(idx+1)+' 最强 '+max.toFixed(1)+' dBm，可向该侧旋转 CPE 对准基站';
  }
  return {score:String(score),advice:advice,spread:spread,max:max,idx:idx,rank:rank,deltaV:deltaV,deltaH:deltaH,raw:raw};
}

function render(data){
  data=data||{};var rawData={};for(var rk in data)rawData[rk]=data[rk];_lastData=rawData;
  var d=data;
  var connected=d.networkStatus==='connected';
  var dlSpeed=num(d.downloadSpeed,0),ulSpeed=num(d.uploadSpeed,0);
  var dl=split(dlSpeed),ul=split(ulSpeed);
var _rsrpRaw=num(d.rsrp,NaN),_sinrRaw=num(d.sinr,NaN),_rsrqRaw=num(d.rsrq,NaN);
// BUGFIX 2026-09-10 21:14: 物理范围校验。collector AT 解析在某些场景下会输出漏负号/单位的异常值
// (如 RSRP=119.9 实为 -119.9, SINR=1413.1 解析串接出错, RSRQ=74.7 单位丢失), 此前无防御直接穿透 toFixed。
// 合法域: RSRP -150~-30 dBm; SINR -20~+50 dB; RSRQ -25~0 dB。越界置 NaN → 显示 -- 兜底。
if(!isNaN(_rsrpRaw)&&(_rsrpRaw>0||_rsrpRaw<-150))_rsrpRaw=NaN;
if(!isNaN(_sinrRaw)&&(_sinrRaw<-20||_sinrRaw>50))_sinrRaw=NaN;
if(!isNaN(_rsrqRaw)&&(_rsrqRaw>0||_rsrqRaw<-25))_rsrqRaw=NaN;
var rsrp=ema('rsrp',_rsrpRaw),sinr=ema('sinr',_sinrRaw),rsrq=ema('rsrq',_rsrqRaw);
  var rpP=isNaN(rsrp)?0:Math.max(0,Math.min(100,((rsrp+130)/70)*100));
  var siP=isNaN(sinr)?0:Math.max(0,Math.min(100,((sinr+10)/40)*100));
  var rqP=isNaN(rsrq)?0:Math.max(0,Math.min(100,((rsrq+25)/15)*100));
  var scoreText=!isNaN(rsrp)&&!isNaN(sinr)?Math.floor(Math.max(0,Math.min(100,(rpP+siP)/2)))+'%':'--';
  var waveS=dlSpeed<1?'10s':dlSpeed<5?'8s':dlSpeed<20?'5s':'3s';
  var dlSc=(0.10+Math.min(0.60,dlSpeed/250)).toFixed(3);
  var ulSc=(0.08+Math.min(0.45,ulSpeed/400)).toFixed(3);
  var ulWaveS=ulSpeed<1?'14s':ulSpeed<5?'10s':ulSpeed<20?'7s':'5s';
  var now=new Date(),w=['日','一','二','三','四','五','六'];
  var op=H(d.operatorZh);if(op==='--')op=H(d.operator);
  var plmn=H(d.plmn);if(op==='--'||op===plmn){var m=plmn2op(plmn);if(m)op=m;}
  if(op==='--')op='未知';
  var rpG=barGrad(rpP),siG=barGrad(siP),rqG=barGrad(rqP);
  var band=H(d.band);if(band!=='--'&&band!=='0'){if(band[0]==='n'||band[0]==='N'){}else if(band.length>2&&band.substring(0,2)==='50')band='n'+band.substring(2);else band='n'+band}else band='--';
  var timeStr=now.getFullYear()+'年'+(now.getMonth()+1)+'月'+now.getDate()+'日 星期'+w[now.getDay()]+' '+String(now.getHours()).padStart(2,'0')+':'+String(now.getMinutes()).padStart(2,'0')+':'+String(now.getSeconds()).padStart(2,'0');
  var temp=fmtTemp(d.temperature),bw=fmtBw(d.bandwidth);
  var antSrc=d.antSource||'none';
  // v64.5 (2026-09-12): 新增 fallback 分支 —— 4 路天线无可靠来源时如实标注，避免被误读为真实分集。
  var srcLabel=antSrc==='gtccinfo'?'GTCCINFO':antSrc==='shm'?'共享内存':antSrc==='ubus'?'QModem':antSrc==='fallback'?'主 RSRP 推算（非真实分集）':'RSRP';
  var html='';
  html+='<div class="topbar"><span class="topbar-title">5G Dashboard</span><span class="topbar-time" id="top-time">'+timeStr+'</span><button class="theme-toggle" onclick="toggleTheme()" title="黑白切换">◐</button></div>';
  html+='<div class="cols">';
  html+='<div class="col">';
  html+='<div class="module"><div class="module-title"><span>运行状态</span><span style="color:var(--green);font-size:11px;font-weight:700">'+(connected?'● Connected':'○ Offline')+'</span></div><div class="speed-row">';
  html+='<div><div class="speed-label">⬇ 下行速率</div><div class="speed-val" id="dl-val">'+dl.i+'<span class="speed-dec">'+dl.d+'</span> <span class="speed-unit">MB/s</span></div></div>';
  html+='<div><div class="speed-label">⬆ 上行速率</div><div class="speed-val" id="ul-val">'+ul.i+'<span class="speed-dec">'+ul.d+'</span> <span class="speed-unit">MB/s</span></div></div></div>';
  html+='<div id="wave-dl" class="wave-wrap" style="transform:scaleY('+dlSc+');transition:transform 0.45s ease-out;animation-duration:'+waveS+'"><div class="wave-inner" style="animation-duration:'+waveS+'"><svg viewBox="0 0 1440 320" preserveAspectRatio="none"><path fill="url(#wgdl)" d="M0,224L48,213.3C96,203,192,181,288,181.3C384,181,480,203,576,224C672,245,768,267,864,261.3C960,256,1056,224,1152,197.3C1248,171,1344,149,1392,138.7L1440,128V320H0Z"/></svg><svg viewBox="0 0 1440 320" preserveAspectRatio="none"><path fill="url(#wgdl2)" opacity="0.55" d="M0,224L48,213.3C96,203,192,181,288,181.3C384,181,480,203,576,224C672,245,768,267,864,261.3C960,256,1056,224,1152,197.3C1248,171,1344,149,1392,138.7L1440,128V320H0Z"/></svg></div></div><div id="wave-ul" class="wave-wrap" style="transform:scaleY('+ulSc+');transition:transform 0.45s ease-out;animation-duration:'+ulWaveS+'"><div class="wave-inner" style="animation-duration:'+ulWaveS+'"><svg viewBox="0 0 1440 320" preserveAspectRatio="none"><path fill="url(#wgul)" d="M0,224L48,213.3C96,203,192,181,288,181.3C384,181,480,203,576,224C672,245,768,267,864,261.3C960,256,1056,224,1152,197.3C1248,171,1344,149,1392,138.7L1440,128V320H0Z"/></svg><svg viewBox="0 0 1440 320" preserveAspectRatio="none"><path fill="url(#wgul2)" opacity="0.55" d="M0,224L48,213.3C96,203,192,181,288,181.3C384,181,480,203,576,224C672,245,768,267,864,261.3C960,256,1056,224,1152,197.3C1248,171,1344,149,1392,138.7L1440,128V320H0Z"/></svg></div></div><svg width="0" height="0"><defs><linearGradient id="wgdl" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#0c4a6e"/><stop offset="1" stop-color="#0284c7"/></linearGradient><linearGradient id="wgdl2" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#0369a1"/><stop offset="1" stop-color="#38bdf8"/></linearGradient><linearGradient id="wgul" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#a5f3fc"/><stop offset="1" stop-color="#67e8f9"/></linearGradient><linearGradient id="wgul2" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#cffafe"/><stop offset="1" stop-color="#22d3ee"/></linearGradient></defs></svg></div>';
  html+='<div class="module"><div class="module-title">网络信息</div><div class="info-list">';
  var voltV=(function(){var v=H(d.voltage);return v==='--'?'--':v+' V';})();
  var rows=[['运营商',op,'#4ade80','#15803d','<rect width="14" height="20" x="5" y="2" rx="2" ry="2"/><path d="M12 18h.01"/>'],['网络模式',H(d.networkMode),'#60a5fa','#2563eb','<path d="M2 20h.01"/><path d="M7 20v-4"/><path d="M12 20v-8"/><path d="M17 20V8"/><path d="M22 4v16"/>'],['信道带宽',bw,'#c084fc','#6d28d9','<path d="M12 15V3"/><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><path d="m7 10 5 5 5-5"/>'],['基站 ID',H(d.cellId),'#f87171','#b91c1c','<path d="M10 12h4"/><path d="M10 8h4"/><path d="M14 21v-3a2 2 0 0 0-4 0v3"/><path d="M6 10H4a2 2 0 0 0-2 2v7a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-2"/><path d="M6 21V5a2 2 0 0 1 2-2h8a2 2 0 0 1 2 2v16"/>'],['IMEI',H(d.imei),'#e879f9','#86198f','<rect width="18" height="12" x="3" y="6" rx="2"/><path d="M7 12h.01M12 12h.01M17 12h.01"/>'],['优先级 (QCI)',H(d.qci),'#722ed1','#4a1d7a','<path d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"/>'],['IPv4',H(d.ipv4),'#9ca3af','#4b5563','<path d="M12 20h.01"/><path d="M2 8.82a15 15 0 0 1 20 0"/><path d="M5 12.859a10 10 0 0 1 14 0"/><path d="M8.5 16.429a5 5 0 0 1 7 0"/>'],['IPv6',H(d.ipv6),'#6b7280','#374151','<path d="M12 20h.01"/><path d="M8.5 16.429a5 5 0 0 1 7 0"/><path d="M5 12.859a10 10 0 0 1 5.17-2.69"/><path d="M19 12.859a10 10 0 0 0-2.007-1.523"/><path d="M2 8.82a15 15 0 0 1 4.177-2.643"/><path d="M22 8.82a15 15 0 0 0-11.288-3.764"/><path d="m2 2 20 20"/>'],['CPU 温度',temp,'#2dd4bf','#0f766e','<path d="M14 4v10.54a4 4 0 1 1-4 0V4a2 2 0 0 1 4 0Z"/>'],['电压',voltV,'#f97316','#9a3412','<path d="M13 2 3 14h7l-1 8 10-12h-7l1-8z"/>'],['SIM',H(d.simStatus),'#a78bfa','#6d28d9','<rect width="18" height="12" x="3" y="6" rx="2"/><path d="M7 15h.01M11 15h.01M15 15h.01M7 19h10"/>'],['载波聚合',H(d.caArfcn)+' / '+H(d.caPci),'#fbbf24','#b45309','<path d="M5 12h14"/><path d="M12 5v14"/><path d="m7 8-5 4 5 4M17 8l5 4-5 4"/>']];
  rows.forEach(function(r,i){html+='<div class="info-row'+(i===rows.length-1?' info-last':'')+'"><div class="info-icon" style="'+iconGrad(r[2],r[3])+'"><svg viewBox="0 0 24 24">'+r[4]+'</svg></div><div class="info-content"><span class="info-label">'+r[0]+'</span><span class="info-value">'+r[1]+'</span></div></div>'});
  html+='</div></div>';
  html+='</div>';
  html+='<div class="col">';
  html+='<div class="module"><div class="module-title"><span>信号强度</span><span class="score-grad">'+scoreText+'</span></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">RSRP</span><span class="prog-val">'+(isNaN(rsrp)?'--':rsrp.toFixed(1)+' dBm')+'</span></div><div class="prog-track"><div class="prog-fill" style="width:'+rpP+'%;--grad-a:'+rpG[0]+'"></div></div></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">SINR</span><span class="prog-val">'+(isNaN(sinr)?'--':sinr.toFixed(1)+' dB')+'</span></div><div class="prog-track"><div class="prog-fill" style="width:'+siP+'%;--grad-a:'+siG[0]+'"></div></div></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">RSRQ</span><span class="prog-val">'+(isNaN(rsrq)?'--':rsrq.toFixed(1)+' dB')+'</span></div><div class="prog-track"><div class="prog-fill" style="width:'+rqP+'%;--grad-a:'+rqG[0]+'"></div></div></div>';
  html+='<div class="module-title" style="margin-top:14px">频段配置</div><div class="metric-row">';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#c084fc','#6d28d9')+'"><svg viewBox="0 0 24 24"><path d="M16.247 7.761a6 6 0 0 1 0 8.478"/><path d="M19.075 4.933a10 10 0 0 1 0 14.134"/><path d="M4.925 19.067a10 10 0 0 1 0-14.134"/><path d="M7.753 16.239a6 6 0 0 1 0-8.478"/><circle cx="12" cy="12" r="2"/></svg></div><div><div class="metric-label">频段</div><div class="metric-value">'+band+'</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#60a5fa','#2563eb')+'"><svg viewBox="0 0 24 24"><path d="m18 8 4 4-4 4"/><path d="M2 12h20"/><path d="m6 8-4 4 4 4"/></svg></div><div><div class="metric-label">带宽</div><div class="metric-value">'+bw+'</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#f472b6','#be185d')+'"><svg viewBox="0 0 24 24"><line x1="4" x2="20" y1="9" y2="9"/><line x1="4" x2="20" y1="15" y2="15"/><line x1="10" x2="8" y1="3" y2="21"/><line x1="16" x2="14" y1="3" y2="21"/></svg></div><div><div class="metric-label">ARFCN</div><div class="metric-value">'+H(d.arfcn)+'</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#fb923c','#c2410c')+'"><svg viewBox="0 0 24 24"><rect width="20" height="8" x="2" y="2" rx="2" ry="2"/><rect width="20" height="8" x="2" y="14" rx="2" ry="2"/><line x1="6" x2="6.01" y1="6" y2="6"/><line x1="6" x2="6.01" y1="18" y2="18"/></svg></div><div><div class="metric-label">PCI</div><div class="metric-value">'+H(d.pci)+'</div></div></div></div>';
  html+='<div class="module-title" style="margin-top:14px">天线分集 <span style="font-size:10px;font-weight:400;color:var(--text2);margin-left:6px">'+srcLabel+'</span></div>';
  html+='<div class="ant-row">';
  var antItems=[['MAIN',d.antMain],['DIV',d.antDiv],['MIMO 1',d.antMimo1],['MIMO 2',d.antMimo2]];
  // BUGFIX 2026-09-10 21:57: 4 路 RSRP 同样做物理范围校验 + 消 -0.0 格式化。
  // 现象: collector 偶发把 antMimo1/2 写成 "0.0" (GTCCINFO 解析漏字段时) → 0.0 物理不可能但 toFixed 直接透出,
  // 柱状图 ((0+130)/70)*100 = 100% 全满绿条, 误导面板。
  // 域: 4 路 RSRP 同样 -150~-30; 越界置 NaN→'--' 兜底; +0 抹零消 -0.0 显型。
  antItems.forEach(function(a){var lbl=a[0],val=a[1],rawN=num(val,NaN),_n=isNaN(rawN)?NaN:((rawN>0||rawN<-150)?NaN:rawN+0);var ac=antColor(_n),ap=antPct(_n);var dispVal=isNaN(_n)?'--':(_n<0?'-':'')+Math.abs(_n).toFixed(1)+' dBm';html+='<div class="ant-box" title="'+lbl+' RSRP"><div class="ant-lbl">'+lbl+'</div><div class="ant-val" style="color:'+ac+'">'+dispVal+'</div><div class="ant-bar"><div class="ant-bar-fill" style="width:'+ap+'%;background:'+ac+'"></div></div></div>';});
  html+='</div></div>';
  html+='<div id="aiqos-card" class="module aiqos-card">'+aiqosCardSkeleton()+'</div>';
  html+='</div>';
  html+='<div class="update-time">🕐 '+now.toLocaleString('zh-CN')+'</div>';
  document.getElementById('content').innerHTML=html;
}

function aiqosCardSkeleton(){
  var opts=[['auto','自动检测（推荐）'],['urban','城市模式 - 最大吞吐'],['rural','乡村模式 - 弱信号优化'],['wilderness','野外模式 - 保活优先'],['high_speed_rail','高铁模式 - 快速切换'],['dark_cci','夜间干扰模式 - 强信号低SINR'],['dark_throttle','夜间限速模式 - 核心网限速']];
  var sel='<select id="aiqos-scenario" class="aiqos-select">';
  for(var i=0;i<opts.length;i++)sel+='<option value="'+opts[i][0]+'">'+opts[i][1]+'</option>';
  sel+='</select>';
  return '<div class="module-title"><span>AiQoS 天线调优</span><span id="aiqos-scenario-chip" class="aiqos-chip">加载中</span></div>'+
    '<div class="aiqos-grid">'+
      '<div class="aiqos-stat"><span>场景</span><strong id="aiqos-scenario-label">--</strong></div>'+
      '<div class="aiqos-stat"><span>目标带宽</span><strong id="aiqos-bw">--</strong></div>'+
    '</div>'+
    '<div class="aiqos-visual">'+
      '<div class="aiqos-ring-wrap">'+
        '<svg viewBox="0 0 120 120" class="aiqos-ring-svg">'+
          '<circle class="aiqos-ring-track" cx="60" cy="60" r="44"></circle>'+
          '<circle id="aiqos-ring" class="aiqos-ring-fill" cx="60" cy="60" r="44" stroke-dasharray="276" stroke-dashoffset="276"></circle>'+
        '</svg>'+
        '<div class="aiqos-ring-center"><strong id="aiqos-score">--</strong><span>SRQI</span></div>'+
      '</div>'+
      '<div class="aiqos-polar-box">'+
        '<div class="aiqos-visual-title">交叉极化差</div>'+
        '<div class="aiqos-polar-row"><span>垂直</span><div class="aiqos-polar-track"><div id="aiqos-delta-v-bar" class="aiqos-polar-fill"></div></div><strong id="aiqos-delta-v">--</strong></div>'+
        '<div class="aiqos-polar-row"><span>水平</span><div class="aiqos-polar-track"><div id="aiqos-delta-h-bar" class="aiqos-polar-fill h"></div></div><strong id="aiqos-delta-h">--</strong></div>'+
        '<div class="aiqos-rank-row"><span>MIMO Rank</span><strong id="aiqos-rank">--</strong></div>'+
      '</div>'+
      '<div class="aiqos-ant-graph">'+
        '<div class="aiqos-ant-graph-head"><span>4 路 RSRP</span><span id="aiqos-pointing">--</span></div>'+
        '<div class="aiqos-ant-graph-bars">'+
          '<div class="aiqos-graph-item"><span>0</span><div class="aiqos-graph-track"><div id="aiqos-graph-0" class="aiqos-graph-fill"></div></div><strong id="aiqos-graph-val-0">--</strong></div>'+
          '<div class="aiqos-graph-item"><span>1</span><div class="aiqos-graph-track"><div id="aiqos-graph-1" class="aiqos-graph-fill"></div></div><strong id="aiqos-graph-val-1">--</strong></div>'+
          '<div class="aiqos-graph-item"><span>2</span><div class="aiqos-graph-track"><div id="aiqos-graph-2" class="aiqos-graph-fill"></div></div><strong id="aiqos-graph-val-2">--</strong></div>'+
          '<div class="aiqos-graph-item"><span>3</span><div class="aiqos-graph-track"><div id="aiqos-graph-3" class="aiqos-graph-fill"></div></div><strong id="aiqos-graph-val-3">--</strong></div>'+
        '</div>'+
      '</div>'+
    '</div>'+
    '<div id="aiqos-direction" class="aiqos-direction">--</div>'+
    '<div class="aiqos-param-grid">'+
      '<div class="aiqos-param"><span>卡尔曼 Q/R</span><strong id="aiqos-param-kf">--</strong></div>'+
      '<div class="aiqos-param"><span>PID Kp/Ki/Kd</span><strong id="aiqos-param-pid">--</strong></div>'+
      '<div class="aiqos-param"><span>最低带宽</span><strong id="aiqos-param-minbw">--</strong></div>'+
      '<div class="aiqos-param"><span>ACK 过滤</span><strong id="aiqos-param-ack">--</strong></div>'+
      '<div class="aiqos-param"><span>切换次数</span><strong id="aiqos-param-handover">--</strong></div>'+
      '<div class="aiqos-param"><span>防呆状态</span><strong id="aiqos-failsafe" class="aiqos-failsafe">--</strong></div>'+
    '</div>'+
    '<div class="aiqos-row">'+sel+'<button class="aiqos-btn" onclick="applyAiqosScenario()">应用场景</button></div>'+
    '<div class="aiqos-row"><span class="aiqos-mode-label">天线模式</span><div id="antenna-mode" class="antenna-mode">'+
      '<button class="mode-btn active" data-mode="auto" onclick="setAntennaMode(\'auto\')">自动</button>'+
      '<button class="mode-btn" data-mode="omni" onclick="setAntennaMode(\'omni\')">全向</button>'+
      '<button class="mode-btn" data-mode="directional" onclick="setAntennaMode(\'directional\')">定向</button>'+
    '</div></div>'+
    '<div id="aiqos-force-row" class="aiqos-force-row" style="display:none">'+
      '<span id="aiqos-force-text">--</span>'+
      '<button class="aiqos-btn force-ok" onclick="confirmForceAntennaMode()">确定按我选择的模式</button>'+
      '<button class="aiqos-btn" onclick="cancelForceAntennaMode()">取消，按算法建议</button>'+
    '</div>'+
    '<div id="aiqos-msg" class="aiqos-hint">--</div>';
}

function aiqosVal(v,unit){var s=H(v);return s==='--'?s:s+(unit||'');}
function aiqosBw(v){var s=H(v);if(s==='--')return s;var n=parseFloat(s);if(isNaN(n))return s;return (n>=1000?Math.round(n/1000):n)+' Mbps';}

// v64.5 重构 (9/11, R1-⑩): 删除 loadAiqosSpatial 死函数——零调用实测,
// spatial 数据早已由 aiqos_all 聚合接口的 spatial 段提供（loadAiqosCard 内 _aiqosSpatial=spatial）。
// 前端请求面 4→3 达成（独立 /aiqos_spatial 接口保留作调试用, 不再被前端轮询）。

function loadAiqosCard(statusData){
  var card=document.getElementById('aiqos-card');
  if(!card)return;
  if(_aiqosLoading)return;
  _aiqosLoading=true;
  // BUGFIX 2026-08-28: 3 个独立 fetch 合并为 1 个聚合接口 aiqos_all
  var p1=statusData?Promise.resolve(statusData):fetch('/cgi-bin/luci/admin/5g_dashboard/aiqos_all').then(function(r){return r.json();}).catch(function(){return {};});
  p1.then(function(all){
    var status = (all && all.status) ? all.status : {};
    var ant = (all && all.antenna) || {};
    var spatial = (all && all.spatial) || {};
    _aiqosStatus=status;
    // v63.5 hotfix (2026-09-11): 模式 stale 保持——接口 mode 空/异常时用上次有效值, 不回落 auto;
    // forced 仅在 mode 为 directional/omni 时生效(避免 forced=true+mode空 时提示行误显示"全向")。
    var _antModeRaw=String(ant.mode||'').replace(/\s+$/,'');
    if(_antModeRaw==='directional'||_antModeRaw==='omni'||_antModeRaw==='auto')_lastValidAiqosMode=_antModeRaw;
    _aiqosMode=_antModeRaw||(_lastValidAiqosMode||'auto');
    _forcedAntennaMode=(ant.forced&&(_aiqosMode==='directional'||_aiqosMode==='omni'))?_aiqosMode:'';
    _pendingForceMode='';
    _userModeAttempts=0;
    _userModeLast='';
    _aiqosSpatial=spatial;
    _aiqosModeLoaded=true;
    _aiqosLoading=false;
    renderAiqosCard();
  }).catch(function(){
    _aiqosLoading=false;
    var msg=document.getElementById('aiqos-msg');
    if(msg)msg.textContent='加载失败';
  });
}

function renderAiqosCard(){
  var card=document.getElementById('aiqos-card');
  if(!card)return;
  var d=_aiqosStatus||{};
  var opts={auto:'自动检测（推荐）',urban:'城市模式 - 最大吞吐',rural:'乡村模式 - 弱信号优化',wilderness:'野外模式 - 保活优先',high_speed_rail:'高铁模式 - 快速切换',dark_cci:'夜间干扰模式 - 强信号低SINR',dark_throttle:'夜间限速模式 - 核心网限速'};
  var key=H(d.current||d.scenario||'auto');
  var label=opts[key]||key;
  var chip=document.getElementById('aiqos-scenario-chip');
  if(chip)chip.textContent=label;
  var sl=document.getElementById('aiqos-scenario-label');
  if(sl)sl.textContent=label;
  var bw=document.getElementById('aiqos-bw');
  // v63.5 hotfix: 带宽 stale 保持——新值无效时保留上次显示(引擎重启窗口不再闪 '--')
  if(bw){
    var _bwv=d.target_bw;
    if(_bwv==null||_bwv===''||H(_bwv)==='--'){ if(_lastBwShown)bw.textContent=aiqosBw(_lastBwShown); }
    else { _lastBwShown=_bwv; bw.textContent=aiqosBw(_bwv); }
  }
  var sel=document.getElementById('aiqos-scenario');
  if(sel)sel.value=key;
  var g=antennaGuidance(_lastData||{});
  var sp=_aiqosSpatial||{};
  var backendScore=H(sp.alignment_score);
  var p=document.getElementById('aiqos-pointing');
  if(p)p.textContent=(backendScore==='--'?g.score:backendScore)==='--'?'--':'已识别';
  var sc=document.getElementById('aiqos-score');
  if(sc)sc.textContent=backendScore==='--'?g.score:backendScore;
  var sv=num(sp.alignment_score,g.score);
  sv=Math.max(0,Math.min(100,sv));
  var ring=document.getElementById('aiqos-ring');
  if(ring){ring.setAttribute('stroke-dashoffset',String(276-Math.round(sv/100*276)));ring.style.stroke=barGrad(sv)[0];}
  var rankEl=document.getElementById('aiqos-rank');
  var rank=H(sp.rank);
  if(rank==='--')rank=g.rank;
  if(rankEl)rankEl.textContent='Rank '+rank;
  var dir=document.getElementById('aiqos-direction');
  if(dir)dir.textContent=(g.advice&&g.advice!=='--')?g.advice:H(sp.advice);
  var live=_lastData||{};
  var antVals=[live.antMain,live.antDiv,live.antMimo1,live.antMimo2];
  var liveOk=0;
  for(var ai=0;ai<4;ai++){if(!isNaN(num(antVals[ai],NaN)))liveOk++;}
  if(liveOk<4&&sp.rsrp_4ch&&sp.rsrp_4ch.length===4)antVals=sp.rsrp_4ch;
  if(liveOk<4&&(!sp.rsrp_4ch||sp.rsrp_4ch.length!==4))antVals=[d.antMain,d.antDiv,d.antMimo1,d.antMimo2];
  for(var i=0;i<4;i++){
    var n=num(antVals[i],NaN);
    var bar=document.getElementById('aiqos-graph-'+i);
    var valEl=document.getElementById('aiqos-graph-val-'+i);
    if(bar){bar.style.width='100%';bar.style.height=(isNaN(n)?0:antPct(n))+'%';bar.style.background=isNaN(n)?'var(--track)':antColor(n);}
    if(valEl)valEl.textContent=isNaN(n)?'--':n.toFixed(1)+' dBm';
  }
  var dv0=isNaN(num(antVals[0],NaN))||isNaN(num(antVals[1],NaN))?NaN:Math.abs(num(antVals[0],NaN)-num(antVals[1],NaN));
  var dh0=isNaN(num(antVals[2],NaN))||isNaN(num(antVals[3],NaN))?NaN:Math.abs(num(antVals[2],NaN)-num(antVals[3],NaN));
  var dv=isNaN(dv0)?num(sp.delta_v_h,g.deltaV):dv0;
  var dh=isNaN(dh0)?num(sp.delta_h_v,g.deltaH):dh0;
  var dvBar=document.getElementById('aiqos-delta-v-bar');
  if(dvBar)dvBar.style.width=(isNaN(dv)?0:Math.max(0,Math.min(100,(dv/15)*100)))+'%';
  var dhBar=document.getElementById('aiqos-delta-h-bar');
  if(dhBar)dhBar.style.width=(isNaN(dh)?0:Math.max(0,Math.min(100,(dh/15)*100)))+'%';
  var dvEl=document.getElementById('aiqos-delta-v');
  if(dvEl)dvEl.textContent=isNaN(dv)?'--':dv.toFixed(1)+' dB';
  var dhEl=document.getElementById('aiqos-delta-h');
  if(dhEl)dhEl.textContent=isNaN(dh)?'--':dh.toFixed(1)+' dB';
  var prof=d.profile||window._aiqosProfile||{};
  var kfEl=document.getElementById('aiqos-param-kf');
  if(kfEl)kfEl.textContent=H(prof.kf_q)+' / '+H(prof.kf_r);
  var pidEl=document.getElementById('aiqos-param-pid');
  if(pidEl)pidEl.textContent=H(prof.pid_kp)+' / '+H(prof.pid_ki)+' / '+H(prof.pid_kd);
  var minEl=document.getElementById('aiqos-param-minbw');
  if(minEl)minEl.textContent=aiqosBw(prof.min_bw);
  var ackEl=document.getElementById('aiqos-param-ack');
  if(ackEl)ackEl.textContent=H(prof.ack_filter);
  var ho=H(d.handover_count);
  if(ho==='--')ho='0';
  var hoEl=document.getElementById('aiqos-param-handover');
  if(hoEl)hoEl.textContent=ho;
  var fsRaw=H(sp.failsafe_status);
  var fsMap={auto:'自动跟踪',omni_locked:'全向锁定',directional_locked:'定向锁定',directional_tracking:'定向跟踪'};
  var fsLabel=fsMap[fsRaw]||fsRaw;
  if(fsRaw==='--'){if(_aiqosMode==='directional')fsLabel='定向跟踪';else if(_aiqosMode==='omni')fsLabel='全向锁定';else fsLabel='自动跟踪';}
  if(_forcedAntennaMode){fsLabel=_forcedAntennaMode==='directional'?'定向锁定（强制）':'全向锁定（强制）';}
  var badge=document.getElementById('aiqos-failsafe');
  if(badge){badge.className='aiqos-failsafe '+(_forcedAntennaMode==='directional'?'ok':_forcedAntennaMode==='omni'?'warn':fsRaw==='directional_tracking'?'ok':fsRaw==='omni_locked'?'warn':'neutral');badge.textContent=fsLabel;}
  var btns=document.querySelectorAll('#antenna-mode .mode-btn');
  for(var j=0;j<btns.length;j++)btns[j].className='mode-btn'+(btns[j].getAttribute('data-mode')===_aiqosMode?' active':'');
  var fs=checkAntennaFailsafe(_lastData||{},_aiqosMode);
  if(fs)_failsafeMsg=fs;
  if(_forcedAntennaMode&&!_failsafeMsg)_failsafeMsg='已强制使用'+(_forcedAntennaMode==='directional'?'定向':'全向')+'模式，防呆不自动回退';
  var msg=document.getElementById('aiqos-msg');
  if(msg)msg.textContent=_failsafeMsg||'';
  var fr=document.getElementById('aiqos-force-row');
  var ft=document.getElementById('aiqos-force-text');
  if(fr&&ft){
    if(_pendingForceMode){
      fr.style.display='flex';
      ft.textContent=_pendingForceMode==='directional'?'系统算法建议全向，你已连续选择定向 2 次':'系统算法建议定向，你已连续选择全向 2 次';
    }else{
      fr.style.display='none';
    }
  }
}
function checkAntennaFailsafe(d,mode){
  if(_forcedAntennaMode===mode)return '';
  var g=antennaGuidance(d);
  if(g.score==='--')return '';
  // BUGFIX 2026-08-28: 自动回退仅限 auto 模式。用户手动选择(omni/directional)时只提示，不自动改配置。
  var isAuto=_aiqosMode==='auto'&&!_forcedAntennaMode;
  // BUGFIX 2026-08-28 15:00: 定向天线极差天然小(1~3dB),spread<3.5 是正常状态,
  // 不能判为"与算法冲突"。只有 4 路都接近且 SINR 极低(说明没对准)才提示回退。
  if(mode==='directional'){
    var dirSinrLow=!isNaN(num(_lastData&&_lastData.sinr,NaN))&&num(_lastData&&_lastData.sinr,NaN)<6;
    if(g.spread<3.5&&dirSinrLow){
      if(_pendingForceMode==='directional')return '四路均衡但 SINR 偏低，疑似未对准基站，请确认是否强制使用定向';
      if(isAuto){
        _directionalEvenCount++;
        if(_directionalEvenCount>=3){
          _directionalEvenCount=0;
          setAntennaMode('omni',{autoReverted:true});
          return '定向模式 SINR 长期偏低，已自动回退至全向';
        }
      }else{
        return '定向模式 SINR 偏低，建议微调仰角/方位对准基站';
      }
    }else{
      _directionalEvenCount=0;
    }
  }else if(mode==='omni'&&g.spread>8){
    if(_pendingForceMode==='omni')return '系统算法建议定向，请确认是否强制使用全向';
    if(!isAuto)return '检测到明显主瓣差异，建议手动切换定向模式';
  }
  return '';
}

function applyAiqosScenario(){
  var sel=document.getElementById('aiqos-scenario');
  if(!sel)return;
  var scenario=sel.value;
  fetch('/cgi-bin/luci/admin/5g_dashboard/aiqos_scenario?scenario='+encodeURIComponent(scenario)).then(function(r){return r.json();}).then(function(d){
    var msg=document.getElementById('aiqos-msg');
    if(d&&d.success){
      if(_aiqosStatus){_aiqosStatus.current=scenario;_aiqosStatus.scenario=scenario;_aiqosStatus.locked=(scenario!=='auto');}
      if(msg)msg.textContent='已应用并锁定：'+scenario;
      refreshAiqosCard();
    }else{
      if(msg)msg.textContent=d&&d.message?d.message:'设置失败';
    }
  }).catch(function(){
    var msg=document.getElementById('aiqos-msg');
    if(msg)msg.textContent='设置失败';
  });
}

function setAntennaMode(mode,opts){
  opts=opts||{};
  var userRequested=!opts.autoReverted&&!opts.force;
  if(userRequested){
    var g=antennaGuidance(_lastData||{});
    // BUGFIX 2026-08-28 15:00: 定向天线极差天然小,spread<3.5 不再视为冲突;
    // 仅 omni 极差>8 提示切定向仍保留。
    var dirSinrLow=!isNaN(num(_lastData&&_lastData.sinr,NaN))&&num(_lastData&&_lastData.sinr,NaN)<6;
    var conflict=(mode==='directional'&&g.spread<3.5&&dirSinrLow)||(mode==='omni'&&g.spread>8);
    if(conflict){
      if(_userModeLast===mode){_userModeAttempts++;}else{_userModeLast=mode;_userModeAttempts=1;}
      if(_userModeAttempts>=2)_pendingForceMode=mode;
    }else{
      _userModeLast=mode;_userModeAttempts=0;_pendingForceMode='';_forcedAntennaMode='';
    }
  }else if(opts.force){
    _forcedAntennaMode=mode;_pendingForceMode='';_userModeAttempts=0;_userModeLast=mode;
  }else{
    if(!_pendingForceMode&&!_forcedAntennaMode){_userModeAttempts=0;_userModeLast='';}
  }
  _aiqosMode=mode;
  _directionalEvenCount=0;
  var btns=document.querySelectorAll('#antenna-mode .mode-btn');
  for(var i=0;i<btns.length;i++)btns[i].className='mode-btn'+(btns[i].getAttribute('data-mode')===mode?' active':'');
  var url='/cgi-bin/luci/admin/5g_dashboard/aiqos_antenna_mode?mode='+encodeURIComponent(mode)+(opts.force?'&force=1':'');
  fetch(url).then(function(r){return r.json();}).then(function(d){
    if(opts.force){_forcedAntennaMode=mode;}
    if(userRequested&&d&&d.forced)_forcedAntennaMode=mode;
    if(userRequested&&(!d||!d.forced))_forcedAntennaMode='';
    if(_userModeAttempts===1&&userRequested){
      _failsafeMsg='系统算法建议'+(mode==='directional'?'全向':'定向')+'，再设置一次同模式可强制使用';
    }else if(opts.force){
      _failsafeMsg='已按你的选择强制使用'+(mode==='directional'?'定向':'全向')+'模式';
    }else if(opts.autoReverted){
      _failsafeMsg=opts.msg||(mode==='directional'?'定向模式与算法冲突，已自动回退至全向':'已按算法建议切换');
    }else{
      _failsafeMsg=d&&d.message==='ok'?'已保存':(d&&d.message?d.message:'');
    }
    renderAiqosCard();
  }).catch(function(){
    _failsafeMsg='设置失败';
    renderAiqosCard();
  });
}

function confirmForceAntennaMode(){
  if(!_pendingForceMode)return;
  setAntennaMode(_pendingForceMode,{force:true});
}

function cancelForceAntennaMode(){
  var mode=_pendingForceMode;
  var suggested=mode==='directional'?'omni':mode==='omni'?'directional':'auto';
  _pendingForceMode='';_userModeAttempts=0;_userModeLast='';_forcedAntennaMode='';
  setAntennaMode(suggested,{autoReverted:true,msg:'已按算法建议切换至'+(suggested==='omni'?'全向':'定向')+'模式'});
}

function refreshAiqosCard(){
  _aiqosStatus=null;
  _aiqosModeLoaded=false;
  _aiqosSpatial=null;
  _aiqosLoading=false;
  loadAiqosCard();
}

var _dataLoading=false;
function loadData(){
  // 防重入。get_5g_info 现只读 /tmp/modem_status.json(实测 0.24s, 无 ubus 阻塞);
  // 采集器速率侧已改 3s 轻量节拍, 轮询相应 10s -> 2s (2026-09-08)
  if(_dataLoading)return;
  _dataLoading=true;
  fetch('/cgi-bin/get_5g_info').then(function(r){return r.json();}).then(function(d){
    d=d||{};
    // BUGFIX: aiqos_status → aiqos_all 聚合接口(1 次返回 status+antenna+spatial)
    fetch('/cgi-bin/luci/admin/5g_dashboard/aiqos_all').then(function(r){return r.json();}).then(function(s){
      s=s||{};
      var st=s.status||{};
      if(H(st.rsrp)!=='--')d.rsrp=st.rsrp;
      if(H(st.rsrq)!=='--')d.rsrq=st.rsrq;
      if(H(st.sinr)!=='--')d.sinr=st.sinr;
      _dataLoading=false;
      render(keepLast(d));
      updateSentinel(d);
      loadAiqosCard(s);
    }).catch(function(){_dataLoading=false;render(keepLast(d));updateSentinel(d);loadAiqosCard();});
  }).catch(function(){_dataLoading=false;render(_lastData||{networkStatus:'disconnected'});updateSentinel(_lastData);loadAiqosCard();});
}

/* 34.5j 哨兵条: 引擎存活 / HNAT hook / 场景 (数据源 get_5g_info.sentinel) */
function updateSentinel(live){
  var card=document.getElementById('aiqos-card');
  if(!card)return;
  var s=(live&&live.sentinel)||{};
  var bar=document.getElementById('sentinel-bar');
  if(!bar){
    bar=document.createElement('div');
    bar.id='sentinel-bar';
    bar.style.cssText='display:flex;gap:12px;font-size:11px;padding:4px 10px;letter-spacing:.3px;flex-wrap:wrap';
    card.insertBefore(bar,card.firstChild);
  }
  var hookOn=(s.hnat_hook==='1'||s.hnat_hook==='enabled');
  var alive=(s.aiqosd_alive==='1');
  function dot(color,txt){return '<span style="color:'+color+'">&#9679; '+txt+'</span>';}
  bar.innerHTML=
    dot(alive?'#22c55e':'#ef4444','引擎'+(alive?'运行':'离线'))+
    dot(hookOn?'#ef4444':'#22c55e','HNAT '+(hookOn?'开启':'关闭'))+
    dot('#94a3b8','场景 '+(s.aiqos_scenario||'--'));
}

loadData();
setInterval(loadData,5000);

// v63.3 独立速率通道 (2026-09-08): 500ms 轻轮询, 只刷新上下行数字与双波浪, 不触碰其他模块。
// 数据源 /cgi-bin/get_speed = /tmp/speed_now.json (采集器 speed_loop 每 0.5s 采样 wwan0 计数器)
var _spdBusy=false;
function speedTick(){
  if(_spdBusy||_dataLoading)return;
  _spdBusy=true;
  fetch('/cgi-bin/get_speed?t='+Date.now()).then(function(r){return r.json();}).then(function(s){
    _spdBusy=false;
    if(!s)return;
    var dlV=num(s.dl,0),ulV=num(s.ul,0);
    var dlP=split(dlV),ulP=split(ulV);
    var eD=document.getElementById('dl-val'),eU=document.getElementById('ul-val');
    if(eD)eD.innerHTML=dlP.i+'<span class="speed-dec">'+dlP.d+'</span> <span class="speed-unit">MB/s</span>';
    if(eU)eU.innerHTML=ulP.i+'<span class="speed-dec">'+ulP.d+'</span> <span class="speed-unit">MB/s</span>';
    var dlSc=(0.10+Math.min(0.60,dlV/250)).toFixed(3),ulSc=(0.08+Math.min(0.45,ulV/400)).toFixed(3);
    var wS=dlV<1?'10s':dlV<5?'8s':dlV<20?'5s':'3s';
    var uS=ulV<1?'14s':ulV<5?'10s':ulV<20?'7s':'5s';
    var wD=document.getElementById('wave-dl'),wU=document.getElementById('wave-ul');
    if(wD){wD.style.transform='scaleY('+dlSc+')';wD.style.animationDuration=wS;
      var i1=wD.querySelector('.wave-inner');if(i1)i1.style.animationDuration=wS;}
    if(wU){wU.style.transform='scaleY('+ulSc+')';wU.style.animationDuration=uS;
      var i2=wU.querySelector('.wave-inner');if(i2)i2.style.animationDuration=uS;}
  }).catch(function(){_spdBusy=false;});
}
setInterval(speedTick,500);
setInterval(function(){var n=new Date(),w=['日','一','二','三','四','五','六'];var hh=String(n.getHours()).padStart(2,'0'),mm=String(n.getMinutes()).padStart(2,'0'),ss=String(n.getSeconds()).padStart(2,'0');var tt=document.getElementById('top-time');if(tt)tt.textContent=n.getFullYear()+'年'+(n.getMonth()+1)+'月'+n.getDate()+'日 星期'+w[n.getDay()]+' '+hh+':'+mm+':'+ss;},1000);
