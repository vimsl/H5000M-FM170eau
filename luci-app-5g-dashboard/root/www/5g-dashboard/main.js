function loadTheme(){var t='dark';try{t=localStorage.getItem('cpe_theme')||'dark'}catch(e){}if(t==='light')document.body.classList.remove('dark');else document.body.classList.add('dark')}
function toggleTheme(){var dark=document.body.classList.toggle('dark');try{localStorage.setItem('cpe_theme',dark?'dark':'light')}catch(e){}}
loadTheme();
function H(v){return (v===null||v===undefined||v===''||v==='undefined'||v==='null'||v==='NaN'||v==='????')?'--':String(v)}
function num(v,fb){var n=parseFloat(v);return isNaN(n)?(fb||0):n}
function split(v){var n=num(v,0).toFixed(2),p=n.split('.');return {i:p[0],d:'.'+p[1]}}
function barGrad(p){if(p>=70)return ['#4ade80','#15803d'];if(p>=40)return ['#facc15','#b45309'];return ['#f87171','#b91c1c']}
function iconGrad(a,b){return '--grad-a:'+a+';--grad-b:'+b}
function antVal(v){v=H(v);return v==='--'?v:v+' dBm'}

var _ema={rsrp:null,rsrq:null,sinr:null};
var EMA_A=0.25;
function ema(key,newVal){
  if(newVal===null||newVal===undefined||isNaN(newVal))return _ema[key];
  if(_ema[key]===null){_ema[key]=newVal;return newVal}
  _ema[key]=_ema[key]+EMA_A*(newVal-_ema[key]);
  return _ema[key];
}

/* Format bytes/s speed: 0 B/s, 1.5 KB/s, 2.3 MB/s */
function fmtSpeed(mbps){
  var v=num(mbps,0);
  if(v<=0) return {val:'0',unit:'Mbps'};
  return {val:v.toFixed(2),unit:'Mbps'};
}

function render(data){
  var d=data||{};
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
  var now=new Date(),w=['\u65E5','\u4E00','\u4E8C','\u4E09','\u56DB','\u4E94','\u516D'];
  var op=H(d.operator); if(op==='--')op='\u672A\u77E5';
  var rpG=barGrad(rpP),siG=barGrad(siP),rqG=barGrad(rqP);
  var band=H(d.band);if(band!=='--'&&band!=='0'){if(band.charAt(0)==='n'||band.charAt(0)==='N'){}else{var bn=parseInt(band,10);if(!isNaN(bn))band='n'+bn}}else{band='--'}
  var timeStr=now.getFullYear()+'\u5E74'+(now.getMonth()+1)+'\u6708'+now.getDate()+'\u65E5 \u661F\u671F'+w[now.getDay()]+' '+String(now.getHours()).padStart(2,'0')+':'+String(now.getMinutes()).padStart(2,'0')+':'+String(now.getSeconds()).padStart(2,'0');
  var temp=H(d.temperature); if(temp!=='--'&&temp.indexOf('\u00B0C')<0)temp+=' \u00B0C';
  var bw=H(d.bandwidth); if(bw!=='--'&&!bw.match(/M$/))bw+=' MHz';
  var html='';
  html+='<div class="topbar"><span class="topbar-title">5G Dashboard</span><span class="topbar-time" id="top-time">'+timeStr+'</span><button class="theme-toggle" onclick="toggleTheme()" title="\u9ED1\u767D\u5207\u6362">\u25D0</button></div>';
  html+='<div class="cols">';
  html+='<div class="col">';
  html+='<div class="module"><div class="module-title"><span>\u8FD0\u884C\u72B6\u6001</span><span style="color:var(--green);font-size:11px;font-weight:700">'+(connected?'\u25CF Connected':'\u25CB Offline')+'</span></div><div class="speed-row">';
  html+='<div><div class="speed-label">\u2B07 \u4E0B\u884C\u901F\u7387</div><div class="speed-val">'+dl.i+'<span class="speed-dec">'+dl.d+'</span> <span class="speed-unit">Mbps</span></div></div>';
  html+='<div><div class="speed-label">\u2B06 \u4E0A\u884C\u901F\u7387</div><div class="speed-val">'+ul.i+'<span class="speed-dec">'+ul.d+'</span> <span class="speed-unit">Mbps</span></div></div></div>';
  html+='<div class="wave-wrap" style="transform:scaleY(0.28);animation-duration:'+waveS+'"><div class="wave-inner"><svg viewBox="0 0 1440 320" preserveAspectRatio="none"><path fill="url(#wg)" d="M0,224L48,213.3C96,203,192,181,288,181.3C384,181,480,203,576,224C672,245,768,267,864,261.3C960,256,1056,224,1152,197.3C1248,171,1344,149,1392,138.7L1440,128V320H0Z"/></svg><svg viewBox="0 0 1440 320" preserveAspectRatio="none"><path fill="url(#wg2)" d="M0,224L48,213.3C96,203,192,181,288,181.3C384,181,480,203,576,224C672,245,768,267,864,261.3C960,256,1056,224,1152,197.3C1248,171,1344,149,1392,138.7L1440,128V320H0Z"/></svg></div></div></div>';
  html+='<svg width="0" height="0"><defs><linearGradient id="wg" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#1d9bf0"/><stop offset="1" stop-color="#00d4ff"/></linearGradient><linearGradient id="wg2" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#00d4ff"/><stop offset="1" stop-color="#1d9bf0"/></linearGradient></defs></svg>';
  html+='<div class="module"><div class="module-title">\u7F51\u7EDC\u4FE1\u606F</div><div class="info-list">';
  var rows=[
    ['\u8FD0\u8425\u5546',op,'#4ade80','#15803d','<rect width="14" height="20" x="5" y="2" rx="2" ry="2"/><path d="M12 18h.01"/>'],
    ['\u7F51\u7EDC\u6A21\u5F0F',H(d.networkMode),'#60a5fa','#2563eb','<path d="M2 20h.01"/><path d="M7 20v-4"/><path d="M12 20v-8"/><path d="M17 20V8"/><path d="M22 4v16"/>'],
    ['\u4FE1\u9053\u5E26\u5BBD',bw,'#c084fc','#6d28d9','<path d="M12 15V3"/><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><path d="m7 10 5 5 5-5"/>'],
    ['\u57FA\u7AD9 ID',H(d.cellId),'#f87171','#b91c1c','<path d="M10 12h4"/><path d="M10 8h4"/><path d="M14 21v-3a2 2 0 0 0-4 0v3"/><path d="M6 10H4a2 2 0 0 0-2 2v7a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-2"/><path d="M6 21V5a2 2 0 0 1 2-2h8a2 2 0 0 1 2 2v16"/>'],
    ['IPv4',H(d.ipv4),'#9ca3af','#4b5563','<path d="M12 20h.01"/><path d="M2 8.82a15 15 0 0 1 20 0"/><path d="M5 12.859a10 10 0 0 1 14 0"/><path d="M8.5 16.429a5 5 0 0 1 7 0"/>'],
    ['IPv6',H(d.ipv6),'#6b7280','#374151','<path d="M12 20h.01"/><path d="M8.5 16.429a5 5 0 0 1 7 0"/><path d="M5 12.859a10 10 0 0 1 5.17-2.69"/><path d="M19 12.859a10 10 0 0 0-2.007-1.523"/><path d="M2 8.82a15 15 0 0 1 4.177-2.643"/><path d="M22 8.82a15 15 0 0 0-11.288-3.764"/><path d="m2 2 20 20"/>'],
    ['\u8BBE\u5907\u6E29\u5EA6',temp,'#2dd4bf','#0f766e','<path d="M14 4v10.54a4 4 0 1 1-4 0V4a2 2 0 0 1 4 0Z"/>'],['\u7535\u538B',H(d.voltage),'#f97316','#9a3412','<path d="M13 2 3 14h7l-1 8 10-12h-7l1-8z"/>'],['SIM',H(d.simStatus),'#a78bfa','#6d28d9','<rect width="18" height="12" x="3" y="6" rx="2"/><path d="M7 15h.01M11 15h.01M15 15h.01M7 19h10"/>'],['\u8F7D\u6CE2\u805A\u5408',H(d.caArfcn)+' / '+H(d.caPci),'#fbbf24','#b45309','<path d="M5 12h14"/><path d="M12 5v14"/><path d="m7 8-5 4 5 4M17 8l5 4-5 4"/>']
  ];
  rows.forEach(function(r,i){html+='<div class="info-row'+(i===rows.length-1?' info-last':'')+'"><div class="info-icon" style="'+iconGrad(r[2],r[3])+'"><svg viewBox="0 0 24 24">'+r[4]+'</svg></div><div class="info-content"><span class="info-label">'+r[0]+'</span><span class="info-value">'+r[1]+'</span></div></div>'});
  html+='</div></div>';
  html+='</div>';
  html+='<div class="col">';
  html+='<div class="module"><div class="module-title"><span>\u4FE1\u53F7\u5F3A\u5EA6</span><span class="score-grad">'+score+'%</span></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">RSRP</span><span class="prog-val">'+rsrp.toFixed(1)+' dBm</span></div><div class="prog-track"><div class="prog-fill" style="width:'+rpP+'%;--grad-a:'+rpG[0]+'"></div></div></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">SINR</span><span class="prog-val">'+sinr.toFixed(1)+' dB</span></div><div class="prog-track"><div class="prog-fill" style="width:'+siP+'%;--grad-a:'+siG[0]+'"></div></div></div>';
  html+='<div class="prog-item"><div class="prog-head"><span class="prog-label">RSRQ</span><span class="prog-val">'+rsrq.toFixed(1)+' dB</span></div><div class="prog-track"><div class="prog-fill" style="width:'+rqP+'%;--grad-a:'+rqG[0]+'"></div></div></div>';
  html+='<div class="module-title" style="margin-top:14px">\u9891\u6BB5\u914D\u7F6E</div><div class="metric-row">';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#c084fc','#6d28d9')+'"><svg viewBox="0 0 24 24"><path d="M16.247 7.761a6 6 0 0 1 0 8.478"/><path d="M19.075 4.933a10 10 0 0 1 0 14.134"/><path d="M4.925 19.067a10 10 0 0 1 0-14.134"/><path d="M7.753 16.239a6 6 0 0 1 0-8.478"/><circle cx="12" cy="12" r="2"/></svg></div><div><div class="metric-label">\u9891\u6BB5</div><div class="metric-value">'+band+'</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#60a5fa','#2563eb')+'"><svg viewBox="0 0 24 24"><path d="m18 8 4 4-4 4"/><path d="M2 12h20"/><path d="m6 8-4 4 4 4"/></svg></div><div><div class="metric-label">\u5E26\u5BBD</div><div class="metric-value">'+H(d.bandwidth)+' MHz</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#f472b6','#be185d')+'"><svg viewBox="0 0 24 24"><line x1="4" x2="20" y1="9" y2="9"/><line x1="4" x2="20" y1="15" y2="15"/><line x1="10" x2="8" y1="3" y2="21"/><line x1="16" x2="14" y1="3" y2="21"/></svg></div><div><div class="metric-label">ARFCN</div><div class="metric-value">'+H(d.arfcn)+'</div></div></div>';
  html+='<div class="metric"><div class="metric-icon" style="'+iconGrad('#fb923c','#c2410c')+'"><svg viewBox="0 0 24 24"><rect width="20" height="8" x="2" y="2" rx="2" ry="2"/><rect width="20" height="8" x="2" y="14" rx="2" ry="2"/><line x1="6" x2="6.01" y1="6" y2="6"/><line x1="6" x2="6.01" y1="18" y2="18"/></svg></div><div><div class="metric-label">PCI</div><div class="metric-value">'+H(d.pci)+'</div></div></div></div>';
  html+='<div class="module-title" style="margin-top:14px">\u5929\u7EBF\u5206\u96C6</div><div class="ant-row"><div class="ant-box" title="\u4E3B\u5929\u7EBF RSRP"><div class="ant-lbl">MAIN</div><div class="ant-val">'+antVal(d.antMain)+'</div></div><div class="ant-box" title="\u5206\u96C6\u5929\u7EBF RSRP"><div class="ant-lbl">DIV</div><div class="ant-val">'+antVal(d.antDiv)+'</div></div><div class="ant-box" title="MIMO 1 RSRP"><div class="ant-lbl">MIMO 1</div><div class="ant-val">'+antVal(d.antMimo1)+'</div></div><div class="ant-box" title="MIMO 2 RSRP"><div class="ant-lbl">MIMO 2</div><div class="ant-val">'+antVal(d.antMimo2)+'</div></div></div>';
  html+='</div>';
  html+='</div><div class="update-time">\uD83D\uDD50 '+now.toLocaleString('zh-CN')+'</div>';
  document.getElementById('content').innerHTML=html;
}
function loadData(){if(window.__MOCK){render(window.__MOCK);return}fetch('/cgi-bin/get_5g_info').then(function(r){return r.json()}).then(function(d){render(d)}).catch(function(){render({networkStatus:'disconnected'})})}
loadData();
setInterval(loadData,5000);
setInterval(function(){var n=new Date(),w=['\u65E5','\u4E00','\u4E8C','\u4E09','\u56DB','\u4E94','\u516D'];var hh=String(n.getHours()).padStart(2,'0'),mm=String(n.getMinutes()).padStart(2,'0'),ss=String(n.getSeconds()).padStart(2,'0');var tt=document.getElementById('top-time');if(tt)tt.textContent=n.getFullYear()+'\u5E74'+(n.getMonth()+1)+'\u6708'+n.getDate()+'\u65E5 \u661F\u671F'+w[n.getDay()]+' '+hh+':'+mm+':'+ss;},1000);