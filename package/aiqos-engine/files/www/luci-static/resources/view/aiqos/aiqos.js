'use strict';
'require view';
'require rpc';
'require form';
'require ui';

var getScenario = rpc.declare({
	object: 'aiqos',
	method: 'get_scenario',
	expect: { '': {} }
});

var setScenario = rpc.declare({
	object: 'aiqos',
	method: 'set_scenario',
	params: ['scenario'],
	expect: { '': {} }
});

var getStatus = rpc.declare({
	object: 'aiqos',
	method: 'get_status',
	expect: { '': {} }
});

var getSpatialInfo = rpc.declare({
	object: 'aiqos',
	method: 'get_spatial_info',
	expect: { '': {} }
});

function fmtBw(kbps) {
	if (!kbps) return '--';
	var mbps = Math.round(parseInt(kbps) / 1000);
	return mbps + ' Mbps';
}

return view.extend({
	load: function() {
		return Promise.all([
			getScenario(),
			getStatus(),
			getSpatialInfo()
		]);
	},

	render: function(data) {
		var scenarioData = data[0] || {};
		var statusData = data[1] || {};
		var spatialData = data[2] || {};

		var currentScenario = scenarioData.current || 'auto';
		var isLocked = scenarioData.locked || false;

		var scenarios = [
			{ key: 'auto',      label: '自动检测（推荐）' },
			{ key: 'urban',     label: '城市模式 - 最大吞吐' },
			{ key: 'rural',     label: '乡村模式 - 弱信号优化' },
			{ key: 'wilderness', label: '野外模式 - 保活优先' },
			{ key: 'high_speed_rail', label: '高铁模式 - 快速切换' },
			{ key: 'dark_cci', label: '夜间干扰模式 - 强信号低SINR' },
			{ key: 'dark_throttle', label: '夜间限速模式 - 核心网限速' }
		];

		var statusTable = E('table', { 'class': 'table' }, [
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '当前场景'),
				E('td', { 'class': 'td' }, E('strong', {}, statusData.scenario || '--'))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '信号强度 RSRP'),
				E('td', { 'class': 'td' }, '%s dBm'.format(statusData.rsrp || '--'))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '信号质量 RSRQ'),
				E('td', { 'class': 'td' }, '%s dB'.format(statusData.rsrq || '--'))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '信噪比 SINR'),
				E('td', { 'class': 'td' }, '%s dB'.format(statusData.sinr || '--'))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '目标带宽'),
				E('td', { 'class': 'td' }, fmtBw(statusData.target_bw))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '切换次数/分钟'),
				E('td', { 'class': 'td' }, statusData.handover_count || '0')
			])
		]);

		var prof = statusData.profile || {};
		var paramTable = E('table', { 'class': 'table' }, [
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '卡尔曼滤波 Q / R'),
				E('td', { 'class': 'td' }, '%s / %s'.format(prof.kf_q || '--', prof.kf_r || '--'))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, 'PID 控制参数 Kp / Ki / Kd'),
				E('td', { 'class': 'td' }, '%s / %s / %s'.format(prof.pid_kp || '--', prof.pid_ki || '--', prof.pid_kd || '--'))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '最低保障带宽'),
				E('td', { 'class': 'td' }, fmtBw(prof.min_bw))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, 'ACK 过滤'),
				E('td', { 'class': 'td' }, prof.ack_filter || '--')
			])
		]);

		var rsrp4 = spatialData.rsrp_4ch || [];
		var spatialTable = E('table', { 'class': 'table' }, [
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '4 路天线 RSRP'),
				E('td', { 'class': 'td' }, rsrp4.length === 4 ? rsrp4.join(' / ') + ' dBm' : '--')
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '垂直/水平极化差'),
				E('td', { 'class': 'td' }, '%s / %s dB'.format(spatialData.delta_v_h || '--', spatialData.delta_h_v || '--'))
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '对准分 SRQI'),
				E('td', { 'class': 'td' }, spatialData.alignment_score || '--')
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, 'MIMO Rank'),
				E('td', { 'class': 'td' }, spatialData.rank || '--')
			]),
			E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, '防呆状态'),
				E('td', { 'class': 'td' }, spatialData.failsafe_status || '--')
			])
		]);

		var select = E('select', { 'class': 'cbi-input-select' });
		scenarios.forEach(function(s) {
			var opt = E('option', { 'value': s.key }, s.label);
			if (s.key === currentScenario) opt.selected = true;
			select.appendChild(opt);
		});

		select.addEventListener('change', function() {
			var val = select.value;
			setScenario(val).then(function() {
				ui.addNotification(null, E('p', {}, '场景已切换为：%s'.format(val)));
			}).catch(function(e) {
				ui.addNotification(null, E('p', { 'class': 'alert-message error' }, e.message));
			});
		});

		return E('div', { 'class': 'cbi-map' }, [
			E('h2', {}, 'AiQoS 智能场景调度'),

			E('div', { 'class': 'cbi-section' }, [
				E('h3', {}, '场景选择'),
				E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title' }, '工作模式'),
					E('div', { 'class': 'cbi-value-field' }, select)
				])
			]),

			E('div', { 'class': 'cbi-section' }, [
				E('h3', {}, '实时状态'),
				statusTable
			]),

			E('div', { 'class': 'cbi-section' }, [
				E('h3', {}, '运行参数'),
				paramTable
			]),

			E('div', { 'class': 'cbi-section' }, [
				E('h3', {}, '天线空间寻星'),
				spatialTable
			])
		]);
	}
});