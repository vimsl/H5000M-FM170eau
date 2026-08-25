// 顶部胶囊型切换栏注入脚本
(function() {
    // 避免重复注入
    if (document.getElementById('dashboard-switcher')) return;

    function injectSwitcher() {
        // 添加Argon主题图标样式
        var iconStyle = document.createElement('style');
        iconStyle.textContent = 
            '/* 适配 iStoreOS Argon 主题侧边栏图标 */' +
            '#mainmenu .menu-admin-5g_dashboard > a::before { content: "\\e6f2" !important; }' +
            '.sidebar .menu-admin-5g_dashboard > a::before { content: "\\e6f2" !important; }';
        document.head.appendChild(iconStyle);

        // 创建容器
        var container = document.createElement('div');
        container.id = 'dashboard-switcher';
        container.innerHTML = '<div class="dashboard-switcher">' +
            '<div class="switcher-track">' +
            '<button id="btn-istore" class="switcher-btn active" onclick="switchToIStore()">原生后台</button>' +
            '<button id="btn-5g" class="switcher-btn" onclick="switchTo5G()">5G 监控看板</button>' +
            '</div>' +
            '</div>';

        // 添加样式
        var style = document.createElement('style');
        style.textContent = 
            '.dashboard-switcher { position: fixed; top: 0; left: 0; right: 0; z-index: 10000; background: #F9FAFB; border-bottom: 1px solid #E5E7EB; padding: 8px 0; display: flex; justify-content: center; }' +
            '.switcher-track { background: #E5E7EB; padding: 4px; border-radius: 8px; display: inline-flex; gap: 0; }' +
            '.switcher-btn { padding: 6px 16px; border: none; background: transparent; border-radius: 6px; cursor: pointer; font-size: 14px; color: #4B5563; transition: all 0.2s ease; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }' +
            '.switcher-btn:hover:not(.active) { background: rgba(255,255,255,0.5); }' +
            '.switcher-btn.active { background: #FFFFFF; color: #111827; font-weight: 600; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }' +
            'body { padding-top: 56px !important; margin-top: 0 !important; }' +
            '.header, #header, header, .luci-header { margin-top: 56px !important; }' +
            '#main-content { margin-top: 56px !important; }';

        document.head.appendChild(style);
        document.body.insertBefore(container, document.body.firstChild);
    }

    // 切换到iStoreOS
    window.switchToIStore = function() {
        var btnIStore = document.getElementById('btn-istore');
        var btn5G = document.getElementById('btn-5g');
        if (btnIStore) btnIStore.classList.add('active');
        if (btn5G) btn5G.classList.remove('active');
        
        // 隐藏5G看板，显示原内容
        var modernDash = document.getElementById('modern-5g-dashboard');
        if (modernDash) modernDash.style.display = 'none';
        
        var originalContent = document.getElementById('original-content');
        if (originalContent) originalContent.style.display = 'block';
    };

    // 切换到5G看板
    window.switchTo5G = function() {
        var btnIStore = document.getElementById('btn-istore');
        var btn5G = document.getElementById('btn-5g');
        if (btn5G) btn5G.classList.add('active');
        if (btnIStore) btnIStore.classList.remove('active');
        
        // 隐藏原内容，显示5G看板
        var modernDash = document.getElementById('modern-5g-dashboard');
        if (modernDash) modernDash.style.display = 'block';
        
        var originalContent = document.getElementById('original-content');
        if (originalContent) originalContent.style.display = 'none';
    };

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', injectSwitcher);
    } else {
        injectSwitcher();
    }
})();
