// 顶部胶囊型切换栏注入脚本
(function() {
    if (document.getElementById('dashboard-switcher')) return;

    function injectSwitcher() {
        var iconStyle = document.createElement('style');
        iconStyle.textContent =
            '#mainmenu .menu-admin-5g_dashboard > a::before { content: "\\e6f2" !important; }' +
            '.sidebar .menu-admin-5g_dashboard > a::before { content: "\\e6f2" !important; }';
        document.head.appendChild(iconStyle);

        var container = document.createElement('div');
        container.id = 'dashboard-switcher';
        container.innerHTML =
            '<div class="dashboard-switcher">' +
            '<div class="switcher-track">' +
            '<button id="btn-istore" class="switcher-btn active" onclick="switchToIStore()">原生后台</button>' +
            '<button id="btn-5g" class="switcher-btn" onclick="switchTo5G()">5G 监控看板</button>' +
            '</div></div>';

        var style = document.createElement('style');
        style.textContent =
            '.dashboard-switcher{position:fixed;top:0;left:0;right:0;z-index:10000;background:#F9FAFB;border-bottom:1px solid #E5E7EB;padding:8px 0;display:flex;justify-content:center}' +
            '.switcher-track{background:#E5E7EB;padding:4px;border-radius:8px;display:inline-flex;gap:0}' +
            '.switcher-btn{padding:6px 16px;border:none;background:transparent;border-radius:6px;cursor:pointer;font-size:14px;color:#4B5563;transition:all .2s ease;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}' +
            '.switcher-btn:hover:not(.active){background:rgba(255,255,255,0.5)}' +
            '.switcher-btn.active{background:#FFF;color:#111827;font-weight:600;box-shadow:0 1px 3px rgba(0,0,0,0.1)}' +
            'body{padding-top:56px!important}' +
            '.header,#header,header,.luci-header{margin-top:56px!important}' +
            '#main-content{margin-top:56px!important}';
        document.head.appendChild(style);
        document.body.insertBefore(container, document.body.firstChild);
    }

    window.switchToIStore = function() {
        var b1 = document.getElementById('btn-istore');
        var b2 = document.getElementById('btn-5g');
        if (b1) b1.classList.add('active');
        if (b2) b2.classList.remove('active');
        var dash = document.getElementById('modern-5g-dashboard');
        if (dash) dash.style.display = 'none';
        var orig = document.getElementById('original-content');
        if (orig) orig.style.display = 'block';
    };

    window.switchTo5G = function() {
        var b1 = document.getElementById('btn-istore');
        var b2 = document.getElementById('btn-5g');
        if (b2) b2.classList.add('active');
        if (b1) b1.classList.remove('active');
        var dash = document.getElementById('modern-5g-dashboard');
        if (dash) dash.style.display = 'block';
        var orig = document.getElementById('original-content');
        if (orig) orig.style.display = 'none';
    };

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', injectSwitcher);
    } else {
        injectSwitcher();
    }
})();
