// Focused JSON workloads motivated by Speedometer 2.1's in-memory TodoMVC store.
// Run with a JS shell, or xpcshell -f. Lower times are better. This does not
// measure Speedometer's DOM, layout, event dispatch, or overall suite score.
(function() {
    var iterations = 2000;
    var checksum = 0;
    function measure(name, records) {
        var text = JSON.stringify(records);
        var samples = [];
        for (var i = 0; i < 100; i++)
            checksum += JSON.parse(text).length;
        for (var sample = 0; sample < 7; sample++) {
            if (typeof gc === 'function')
                gc();
            var start = Date.now();
            for (var i = 0; i < iterations; i++)
                checksum += JSON.parse(text).length;
            samples.push(Date.now() - start);
        }
        samples.sort(function(a, b) { return a - b; });
        print(name + ': median=' + samples[3] + ' ms; samples=' + samples.join(','));
    }
    var todos = [], unicode = [], collisions = [], unique = [];
    for (var i = 0; i < 100; i++) {
        todos.push({ id: i, title: 'Something to do ' + i, completed: false });
        unicode.push({ '\u0101name': i, '\u03bbvalue': 'value', '\u4e2d': false });
        collisions.push({ item: i, identifier: i + 1, index: i + 2 });
        var record = {};
        record['uniqueName' + i] = i;
        unique.push(record);
    }
    measure('TodoMVC-shaped records', todos);
    measure('two-byte names', unicode);
    measure('cache collisions', collisions);
    measure('unique names', unique);
    measure('small parse', [todos[0]]);
    print('checksum=' + checksum);
})();
