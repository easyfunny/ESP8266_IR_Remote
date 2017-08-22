(function() {
	var jsonOk = function(j) {
		var pages = j['pages'];
		var rows, row, keys, key;

		//draw keys
		var divpagetpl = '<div class="main active">\n';
		var divkeygrouptpl = '<div class="btn-box">\n';
		var keytpl = '<a class="btn" c="bits={BITS}&amp;protocol={PRO}&amp;code={CODE}">{NAME}</a>\n';
		var x, key, _h = '';
		for (var i = 0; i < pages.length; i++) {
			//draw pages
			if (i > 0) {
				_h += divpagetpl.replace('active', '');
			} else {
				_h += divpagetpl;
			}

			rows = pages[i]['rows'];
			for (var j = 0; j < rows.length; j++) {
				row = rows[j];
				keys = row['keys'];
				_h += divkeygrouptpl;
				for (var k = 0; k < keys.length; k++) {
					key = keys[k];
					_h += keytpl.replace('{CODE}', "" + key.code).replace('{BITS}', "" + key.bits).replace('{PRO}', "" + key.protocol).replace('{NAME}', "" + key.name);
				}
				_h += '</div>\n'
			}
			_h += "</div>\n";
		}
		$("#_btnList").html(_h);


		var tabtpl = '<a class="active">{NAME}</a>\n';
		var c = 'class="active"';
		_h = '';
		for (var i = 0; i < pages.length; i++) {
			if (i > 0) {
				_h += tabtpl.replace('{NAME}', pages[i]['name']);
			} else {
				_h += tabtpl.replace('{NAME}', pages[i]['name']).replace(c, '');
			}
		}
		if (pages.length > 1) {
			$("#_tabList").html(_h);
		}
	};

	var bindEvent = function() {
		var $tab_a = $('.tab>a');
		var $main = $('.main');
		$tab_a.on({
			click: function() {
				if ($(this).hasClass('active')) return;
				$(this).siblings().removeClass('active').end().addClass('active');
				$main.removeClass('active').eq($(this).index()).addClass('active');
			}
		});
		var $btn = $('.btn');
		$btn.on({
			click: function(e) {
				$(this).animate({
					opacity: '0.4'
				}, "fast");
				$(this).animate({
					opacity: '1'
				}, "fast");
				var url = "ir?" + $(this).attr('c');
				$.get(url);
			}
		});
	};

	$.ajax({
		async: false,
		url: 'config.json',
		type: "GET",
		dataType: 'json',
		//jsonp: 'callback',
		timeout: 5000,
		success: function(json) {
			jsonOk(json);
			bindEvent();
		}
	});
})();