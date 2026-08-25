function loadTheme(){var t='dark';try{t=localStorage.getItem('cpe_theme')||'dark'}catch(e){}if(t==='light')document.body.classList.remove('dark');else document.body.classList.add('dark')} function toggleTheme(){var dark=document.body.classList.toggle('dark');try{localStorage.setItem('cpe_theme',dark?'dark':'light')}catch(e){}} loadTheme();
function H(v){return (v===null||v===undefined||v===''||v==='undefined'||v==='null'||v==='NaN'||v==='????')?'--':String(v)}
function num(v,fb){var n=parseFloat(v);return isNaN(n)?(fb||0):n}
function split(v){var n=num(v,0).toFixed(2),p=n.split('.');return {i:p[0],d:'.'+p[1]}}
function barGrad(p){if(p>=70)return ['#4ade80','#15803d'];if(p>=40)return ['#facc15','#b45309'];return ['#f87171','#b91c1c']}
function iconGrad(a,b){return '--grad-a:'+a+';--grad-b:'+b}
function antColor(v){var n=num(v,NaN);if(isNaN(n))return 'var(--text2)';var p=Math.max(0,Math.min(100,((n+130)/70)*100));if(p>=70)return '#4ade80';if(p>=40)return '#facc15';return '#f87171'}
function antPct(v){var n=num(v,NaN);if(isNaN(n))return 0;return Math.max(0,Math.min(100,((n+130)/70)*100))}
var _ema={rsrp:null,rsrq:null,sinr:null};var EMA_A=0.25;
function ema(key,newVal){if(newVal===null||newVal===undefined||isNaN(newVal))return _ema[key];if(_ema[key]===null){_ema[key]=newVal;return newVal}_ema[key]=_ema[key]+EMA_A*(newVal-_ema[key]);return _ema[key]}
var PLMN_MAP={'46000':'中国移动','46002':'中国移动','46007':'中国移动','46008':'中国移动','46001':'中国联通','46006':'中国联通','46009':'中国联通','46003':'中国电信','46005':'中国电信','46011':'中国电信','46015':'中国广电','46004':'中国广电','46048':'中国广电','46049':'中国电信','46099':'中国电信'};
function plmn2op(p){return PLMN_MAP[p]||''}
function fmtBw(v){var s=H(v);if(s==='--')return s;s=s.replace(/M\s*$/i,'').replace(/\s*MHz$/i,'').trim();return s+' MHz'}
function fmtTemp(v){var s=H(v);if(s==='--')return s;if(s.indexOf('\u00b0C')<0)s+=' \u00b0C';return s}

var _lastData=null;
var _aiqosStatus=null;
var _aiqosMode='auto';
var _aiqosSpatial=null;
var _aiqosLoading=false;
var _aiqosModeLoaded=false;
var _directionalEvenCount=0;
var _failsafeMsg='';

function antennaGuidance(d){
  var raw=[num(d.antMain,NaN),num(d.antDiv,NaN),num(d.antMimo1,NaN),num(d.antMimo2,NaN)];
  var clean=[];
  for(var i=0;i<raw.length;i++){if(!isNaN(raw[i])&&raw[i]>-150&&raw[i]<0)clean.push({v:raw[i],i:i});}
  if(!clean.length)return {score:'--',advice:'等待天线数据',spread:0,max:0,idx:-1,rank:'--',deltaV:0,deltaH:0,raw:raw};
  var min=clean[0].v,max=clean[0].v,idx=clean[0].i;
  for(var j=1;j<clean.length;j++){if(clean[j].v>max){max=clean[j].v;idx=clean[j].i;}if(clean[j].v<min)min=clean[j].v;}
  var spread=max-min;
  var sinr=num(d.sinr,NaN);
  var score=Math.round(Math.max(0,Math.min(100,((max+130)/70)*55+(isNaN(sinr)?0:Math.max(0,Math.min(45,sinr*3))))));
  var rank=spread<3.5?4:spread<6?3:spread<10?2:1;
  var deltaV=isNaN(raw[0])||isNaN(raw[1])?0:Math.abs(raw[0]-raw[1]);
  var deltaH=isNaN(raw[2])||isNaN(raw[3])?0:Math.abs(raw[2]-raw[3]);
  var advice;
  if(spread<3.5)advice='四路天线均衡，适合全向模式；若需指向请检查 MIMO 空间流';
  else if(!isNaN(sinr)&&sinr<3&&max>-80)advice='强信号低 SINR，疑似同频反射，建议移动 CPE 位置';
  else if(clean.length>=2&&Math.abs(clean[0].v-clean[1].v)>6)advice='垂直极化偏弱，建议向上微调仰角';
  else if(clean.length>=4&&Math.abs(clean[2].v-clean[3].v)>6)advice='水平极化偏弱，建议左右微调方位';
  else advice='Rx'+(idx+1)+' 最强 '+max.toFixed(1)+' dBm，可向该侧旋转 CPE 对准基站';
  return {score:String(score),advice:advice,spread:spread,max:max,idx:idx,rank:rank,deltaV:deltaV,deltaH:deltaH,raw:raw};
}

function render(data){
  data=data||{};_lastData=data;
  var d=data;
  var connected=d.networkStatus==='connected';
  var dlSpeed=num(d.downloadSpeed,0),ulSpeed=num(d.uploadSpeed,0);
  var dl=split(dlSpeed),ul=split(ulSpeed);
  var rsrp=ema('rsrp',num(d.rsrp,-130));
  var sinr=ema('sinr',num(d.sinr,-10));
  var rsrq=ema('rsrq',num(d.rsrq,-25));
  var rpP=Math.max(0,Math.min(100,((rsrp+130)/70)*100));
  var siP=Math.max(0,Math.min(100,((sinr+10)/40)*100));
  var rqP=Math.max(0,Math.min(100,((rsrq+25)/15)*100));
  var score=Math.floor(Math.max(0,Math.min(100,(rpP+siP)/2)));
  var waveS=dlSpeed<1?'10s':dlSpeed<5?'8s':dlSpeed<20?'5s':'3s';
  var now=new Date(),w=['日','一','二','三','四','五','六'];
  var op=H(d.operatorZh);if(op==='--')op=H(d.operator);
  var plmn=H(d.plmn);if(op==='--'||op===plmn){var m=plmn2op(plmn);if(m)op=m;}
  if(op==='--')op='未知';
  var rpG=barGrad(rpP),siG=barGrad(siP),rqG=barGrad(rqP);
  var band=H(d.band);if(band!=='--'&&band!=='0'){if(band[0]==='n'||band[0]==='N'){}else if(band.length>2&&band.substring(0,2)==='50')band='n'+band.substring(2);else band='n'+band}else band='--';
  var timeStr=now.getFullYear()+'年'+(now.getMonth()+1)+'月'+now.getDate()+'日 星期'+w[now.getDay()]+' '+String(now.getHours()).padStart(2,'0')+':'+String(now.getMinutes()).padStart(2,'0')+':'+String(now.getSeconds()).padStart(2,'0');
  var temp=fmtTemp(d.temperature),bw=fmtBw(d.bandwidth);
  var antSrc=d.antSource||'none';
  var srcLabel=antSrc==='gtccinfo'?'GTCCINFO':antSrc==='shm'?'共享内存':antSrc==='ubus'?'QModem':'RSRP';
  var html='';
  html+='<div class="topbar"><span class="topbar-title">5G Dashboard</span><span class="topbar-time" id="top-time">'+timeStr+'</span><button class="theme-toggle" onclick="toggleTheme()" title="黑白切换">◐</button></div>';
  html+='<div class="cols">';
  html+='<div class="col">';
  html+='<div class="module"><div class="module-title"><span>运行状态</span><span style="color:var(--green);font-size:11px;font-weight:700">'+(connected?'● Connected':'○ Offline')+'</span></div><div class="speed-row">';
  html+='<div><div class="speed-label">⬇ 下行速率</div><div class="speed-val">'+dl.i+'<span class="speed-dec">'+dl.d+'</span> <span class="speed-unit">Mbps</span></div></div>';
  html+='<div><div class="speed-label">⬆ 上行速率</div><div class="speed-val">'+ul.i+'<span class="speed-dec">'+ul.d+'</span> <span class="speed-unit">Mbps</span></div></div></div>';
  html+='<div class="wave-wrap" style="transform:scaleY(0.28);animation-duration:'+waveS+'"><div class="wave-inner"><svg viewBox="0 0 1440 320" preserveAspectRatio="none"><path fill="url(#wg)" d="M0,224L48,213.3C96,203,192,181,288,181.3C384,181,480,203,576,224C672,245,768,267,864,261.3C960,256,1056,224,1152,197.3C1248,171,1344,149,1392,138.7L1440,128V320H0Z"/></svg><svg viewBox="0 0 1440 320" preserveAspectRatio="none"><path fill="url(#wg2)" d="M0,224L48,213.3C96,203,192,181,288,181.3C384,181,480,203,576,224C672,245,768,267,864,261.3C960,256,1056,224,1152,197.3C1248,171,1344,149,1392,138.7L1440,128V320H0Z"/></svg></div></div><svg width="0" height="0"><defs><linearGradient id="wg" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#1d9bf0"/><stop offset="1" stop-color="#00d4ff"/></linearGradient><linearGradient id="wg2" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#00d4ff"/><stop offset="1" stop-color="#1d9bf0"/></linearGradient></defs></svg></div>';
  html+='<div class="module"><div class="module-title">网络信息</div><div class="info-list">';
  var rows=[['运营商',op,'#4ade80','#15803d','<rect width="14" height="20" x="5" y="2" rx="2" ry="2"/><path d="M12 18h.01"/>'],['网络模式',H(d.networkMode),'#60a5fa','#2563eb','<path d="M2 20h.01"/><path d="M7 20v-4"/><path d="M12 20v-8"/><path d="M17 20V8"/><path d="M22 4v16"/>'],['信道带宽',bw,'#c084fc','#6d28d9','<path d="M12 15V3"/><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><path d="m7 10 5 5 5-5"/>'],['基站 ID',H(d.cellId),'#f87171','#b91c1c','<path d="M10 12h4"/><path d="M10 8h4"/><path d="M14 21v-3a2 2 0 0 0-4 0v3"/><path d="M6 10H4a2 2 0 0 0-2 2v7a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-2"/><path d="M6 21V5a2 2 0 0 1 2-2h8a2 2 0 0 1 2 2v16"/>'],['IMEI',H(d.imei),'#e879f9','#86198f','<rect width="18" height="12" x="3" y="6" rx="2"/><path d="M7 12h.01M12 12h.01M17 12h.01"/>'],['优先级 (QCI)',H(d.qci),'#722ed1','#4a1d7a','<path d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"/>'],['IPv4',H(d.ipv4),'#9ca3af','#4b5563','<path d="M12 20h.01"/><path d="M2 8.82a15 15 0 0 1 20 0"/><path d="M5 12.859a10 10 0 0 1 14 0"/><path d="M8.5 16.429a5 5 0 0 1 7 0"/>'],['IPv6',H(d.ipv6),'#6b7280','#374151','<path d="M12 20h.01"/><path d="M8.5 16.429a5 5 0 0 1 7 0"/><path d="M5 12.859a10 10 0 0 1 5.17-2.69"/><path d="M19 12.859a10 10 0 0 0-2.007-1.523"/><path d="M2 8.82a15 15 0 0 1 4.177-2.643"/><path d="M22 8.82a15 15 0 0 0-11.288-3.764"/><path d="m2 2 20 20"/>'],['设备温度',temp,'#2dd4bf','#0f766e','<path d="M14 4v10.54a4 4 0 1 1-4 0V4a2 2 0 0 1 4 0Z"/>'],['电压',H(d.voltage),'#f97316','#9a3412','<path d="M13 2 3 14h7l-1 8 10-12h-7l1-8z"/>'],['SIM',H(d.simStatus),'#a78bfa','#6d28d9','<rect width="18" height="12" x="3" y="6" rx="2"/><path d="M7 15h.01M11 15h.01M15 15h.01M7 19h10"/>'],['载波聚合',H(d.caArfcn)+' / '+H(d.caPci),'#fbbf24','#b45309','<path d="M5 12h14"/><path d="M12 5v14"/><path d="m7 8-5 4 5 4M17 8l5 4-5 4"/>']];
  rows.forEach(function(r,i){html+='<div class="info-row'+(i===rows.length-1?' info-last':'')+'"><div class="info-icon" style="'+iconGrad(r[2],r[3])+'"><svg viewBox="0 0 24 24">'+r[4]+'</svg></div><div class="info-content"><span class="info-label">'+r[0]+'</span><span class="info-value">'+r[1]+'</span></div></div>'});
  html+='</div></div>';
  html+='</div>';
  html+='<div class="col">';
  html+='<div class="module"><div class="module-title"><span>信号强度</span><span class="score-grad">'+score+'%</span></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">RSRP</span><span class="prog-val">'+rsrp.toFixed(1)+' dBm</span></div><div class="prog-track"><div class="prog-fill" style="width:'+rpP+'%;--grad-a:'+rpG[0]+'"></div></div></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">SINR</span><span class="prog-val">'+sinr.toFixed(1)+' dB</span></div><div class="prog-track"><div class="prog-fill" style="width:'+siP+'%;--grad-a:'+siG[0]+'"></div></div></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">RSRQ</span><span class="prog-val">'+rsrq.toFixed(1)+' dB</span></div><div class="prog-track"><div class="prog-fill" style="width:'+rqP+'%;--grad-a:'+rqG[0]+'"></div></div></div>';
  html+='<div class="module-title" style="margin-top:14px">频段配置</div><div class="metric-row">';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#c084fc','#6d28d9')+'"><svg viewBox="0 0 24 24"><path d="M16.247 7.761a6 6 0 0 1 0 8.478"/><path d="M19.075 4.933a10 10 0 0 1 0 14.134"/><path d="M4.925 19.067a10 10 0 0 1 0-14.134"/><path d="M7.753 16.239a6 6 0 0 1 0-8.478"/><circle cx="12" cy="12" r="2"/></svg></div><div><div class="metric-label">频段</div><div class="metric-value">'+band+'</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#60a5fa','#2563eb')+'"><svg viewBox="0 0 24 24"><path d="m18 8 4 4-4 4"/><path d="M2 12h20"/><path d="m6 8-4 4 4 4"/></svg></div><div><div class="metric-label">带宽</div><div class="metric-value">'+bw+'</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#f472b6','#be185d')+'"><svg viewBox="0 0 24 24"><line x1="4" x2="20" y1="9" y2="9"/><line x1="4" x2="20" y1="15" y2="15"/><line x1="10" x2="8" y1="3" y2="21"/><line x1="16" x2="14" y1="3" y2="21"/></svg></div><div><div class="metric-label">ARFCN</div><div class="metric-value">'+H(d.arfcn)+'</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#fb923c','#c2410c')+'"><svg viewBox="0 0 24 24"><rect width="20" height="8" x="2" y="2" rx="2" ry="2"/><rect width="20" height="8" x="2" y="14" rx="2" ry="2"/><line x1="6" x2="6.01" y1="6" y2="6"/><line x1="6" x2="6.01" y1="18" y2="18"/></svg></div><div><div class="metric-label">PCI</div><div class="metric-value">'+H(d.pci)+'</div></div></div></div>';
  html+='<div class="module-title" style="margin-top:14px">天线分集 <span style="font-size:10px;font-weight:400;color:var(--text2);margin-left:6px">'+srcLabel+'</span></div>';
  html+='<div class="ant-row">';
  var antItems=[['MAIN',d.antMain],['DIV',d.antDiv],['MIMO 1',d.antMimo1],['MIMO 2',d.antMimo2]];
  antItems.forEach(function(a){var lbl=a[0],val=a[1],n=num(val,NaN),ac=antColor(val),ap=antPct(val);var dispVal=isNaN(n)?'--':n.toFixed(1)+' dBm';html+='<div class="ant-box" title="'+lbl+' RSRP"><div class="ant-lbl">'+lbl+'</div><div class="ant-val" style="color:'+ac+'">'+dispVal+'</div><div class="ant-bar"><div class="ant-bar-fill" style="width:'+ap+'%;background:'+ac+'"></div></div></div>';});
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
    '<div id="aiqos-msg" class="aiqos-hint">--</div>';
}

function aiqosVal(v,unit){var s=H(v);return s==='--'?s:s+(unit||'');}
function aiqosBw(v){var s=H(v);if(s==='--')return s;var n=parseFloat(s);if(isNaN(n))return s;return (n>=1000?Math.round(n/1000):n)+' Mbps';}

function loadAiqosCard(){
  var card=document.getElementById('aiqos-card');
  if(!card)return;
  if(_aiqosStatus&&_aiqosModeLoaded){renderAiqosCard();return;}
  if(_aiqosLoading)return;
  _aiqosLoading=true;
  var p1=fetch('/cgi-bin/luci/admin/5g_dashboard/aiqos_status').then(function(r){return r.json();}).catch(function(){return {};});
  var p2=fetch('/cgi-bin/luci/admin/5g_dashboard/aiqos_antenna').then(function(r){return r.json();}).catch(function(){return {};});
  var p3=fetch('/cgi-bin/luci/admin/5g_dashboard/aiqos_spatial').then(function(r){return r.json();}).catch(function(){return {};});
  Promise.all([p1,p2,p3]).then(function(arr){
    _aiqosStatus=arr[0]||{};
    _aiqosMode=String((arr[1]&&arr[1].mode)||'auto').replace(/\s+$/,'');
    _aiqosSpatial=arr[2]||{};
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
  if(bw)bw.textContent=aiqosBw(d.target_bw);
  var sel=document.getElementById('aiqos-scenario');
  if(sel)sel.value=key;
  var g=antennaGuidance(_lastData||{});
  var sp=_aiqosSpatial||{};
  var p=document.getElementById('aiqos-pointing');
  if(p)p.textContent=g.score==='--'?'--':'已识别';
  var sc=document.getElementById('aiqos-score');
  if(sc)sc.textContent=g.score;
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
  var antVals=(sp.rsrp_4ch&&sp.rsrp_4ch.length===4)?sp.rsrp_4ch:[d.antMain,d.antDiv,d.antMimo1,d.antMimo2];
  for(var i=0;i<4;i++){
    var n=num(antVals[i],NaN);
    var bar=document.getElementById('aiqos-graph-'+i);
    var valEl=document.getElementById('aiqos-graph-val-'+i);
    if(bar){bar.style.width=(isNaN(n)?0:antPct(n))+'%';bar.style.background=isNaN(n)?'var(--track)':antColor(n);}
    if(valEl)valEl.textContent=isNaN(n)?'--':n.toFixed(1)+' dBm';
  }
  var dv=num(sp.delta_v_h,g.deltaV);
  var dh=num(sp.delta_h_v,g.deltaH);
  var dvBar=document.getElementById('aiqos-delta-v-bar');
  if(dvBar)dvBar.style.width=Math.max(0,Math.min(100,(dv/15)*100))+'%';
  var dhBar=document.getElementById('aiqos-delta-h-bar');
  if(dhBar)dhBar.style.width=Math.max(0,Math.min(100,(dh/15)*100))+'%';
  var dvEl=document.getElementById('aiqos-delta-v');
  if(dvEl)dvEl.textContent=dv.toFixed(1)+' dB';
  var dhEl=document.getElementById('aiqos-delta-h');
  if(dhEl)dhEl.textContent=dh.toFixed(1)+' dB';
  var prof=d.profile||{};
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
  if(fsRaw==='--'){if(_aiqosMode==='directional')fsLabel=g.spread>=3.5?'定向跟踪':'全向锁定';else if(_aiqosMode==='omni')fsLabel='全向锁定';else fsLabel='自动跟踪';}
  var badge=document.getElementById('aiqos-failsafe');
  if(badge){badge.className='aiqos-failsafe '+(fsRaw==='directional_tracking'?'ok':fsRaw==='omni_locked'?'warn':'neutral');badge.textContent=fsLabel;}
  var btns=document.querySelectorAll('#antenna-mode .mode-btn');
  for(var j=0;j<btns.length;j++)btns[j].className='mode-btn'+(btns[j].getAttribute('data-mode')===_aiqosMode?' active':'');
  var fs=checkAntennaFailsafe(_lastData||{},_aiqosMode);
  if(fs)_failsafeMsg=fs;
  var msg=document.getElementById('aiqos-msg');
  if(msg)msg.textContent=_failsafeMsg||'';
}

function checkAntennaFailsafe(d,mode){
  var g=antennaGuidance(d);
  if(g.score==='--')return '';
  if(mode==='directional'){
    if(g.spread<3.5){_directionalEvenCount++;}else{_directionalEvenCount=0;}
    if(_directionalEvenCount>=3){
      _directionalEvenCount=0;
      setAntennaMode('omni',true);
      return '定向模式误设已自动回退至全向';
    }
  }else if(mode==='omni'&&g.spread>8){
    return '检测到明显主瓣差异，建议手动切换定向模式';
  }
  return '';
}

function applyAiqosScenario(){
  var sel=document.getElementById('aiqos-scenario');
  if(!sel)return;
  var scenario=sel.value;
  fetch('/cgi-bin/luci/admin/5g_dashboard/aiqos_scenario?scenario='+encodeURIComponent(scenario)).then(function(r){return r.json();}).then(function(d){
    var msg=document.getElementById('aiqos-msg');
    if(msg)msg.textContent=d&&d.message==='ok'?'已应用':(d&&d.message?d.message:'');
    if(d&&d.success)refreshAiqosCard();
  }).catch(function(){
    var msg=document.getElementById('aiqos-msg');
    if(msg)msg.textContent='设置失败';
  });
}

function setAntennaMode(mode,autoReverted){
  _aiqosMode=mode;
  _directionalEvenCount=0;
  var btns=document.querySelectorAll('#antenna-mode .mode-btn');
  for(var i=0;i<btns.length;i++)btns[i].className='mode-btn'+(btns[i].getAttribute('data-mode')===mode?' active':'');
  fetch('/cgi-bin/luci/admin/5g_dashboard/aiqos_antenna_mode?mode='+encodeURIComponent(mode)).then(function(r){return r.json();}).then(function(d){
    _failsafeMsg=autoReverted?('定向模式误设已自动回退至全向'):(d&&d.message==='ok'?'已保存':(d&&d.message?d.message:''));
    renderAiqosCard();
  }).catch(function(){
    _failsafeMsg='设置失败';
    renderAiqosCard();
  });
}

function refreshAiqosCard(){
  _aiqosStatus=null;
  _aiqosModeLoaded=false;
  _aiqosSpatial=null;
  _aiqosLoading=false;
  loadAiqosCard();
}

function loadData(){
  if(window.__MOCK){render(window.__MOCK);loadAiqosCard();return;}
  fetch('/cgi-bin/get_5g_info').then(function(r){return r.json();}).then(function(d){render(d);loadAiqosCard();}).catch(function(){render({networkStatus:'disconnected'});loadAiqosCard();});
}

loadData();
setInterval(loadData,5000);
setInterval(function(){var n=new Date(),w=['日','一','二','三','四','五','六'];var hh=String(n.getHours()).padStart(2,'0'),mm=String(n.getMinutes()).padStart(2,'0'),ss=String(n.getSeconds()).padStart(2,'0');var tt=document.getElementById('top-time');if(tt)tt.textContent=n.getFullYear()+'年'+(n.getMonth()+1)+'月'+n.getDate()+'日 星期'+w[n.getDay()]+' '+hh+':'+mm+':'+ss;},1000);