'use strict';
'require view';

return view.extend({
	render: function() {
		var port = '19999';
		var url = window.location.protocol + '//' + window.location.hostname + ':' + port;
		return E('div', { 'style': 'width: 100%; height: calc(100vh - 120px); padding: 0;' }, [
			E('iframe', {
				'src': url,
				'style': 'width: 100%; height: 100%; border: none; border-radius: 8px;'
			})
		]);
	}
});
