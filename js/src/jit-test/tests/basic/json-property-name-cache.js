// Repeated names, collisions, encodings, and escaped names must all agree.
var keys = ['id', 'title', 'completed', 'items', 'item', '', 'identifier',
            'i', 'a', 'same', 'samesize', '\u00e9', '\u0101', '\ud800',
            'quote"key', 'slash\\key', 'line\nkey', '__proto__'];
var records = [];
for (var i = 0; i < 100; i++) {
    var record = Object.create(null);
    for (var k = 0; k < keys.length; k++)
        record[keys[k]] = i + k;
    records.push(record);
}
var text = JSON.stringify(records);
for (var repeat = 0; repeat < 30; repeat++) {
    var parsed = JSON.parse(text);
    assertEq(parsed.length, records.length);
    for (var i = 0; i < parsed.length; i++) {
        for (var k = 0; k < keys.length; k++)
            assertEq(parsed[i][keys[k]], records[i][keys[k]]);
    }
}
assertEq(JSON.parse('[{"title":1},{"titleLonger":2}]')[1].titleLonger, 2);
assertEq(JSON.parse('[{"titleLonger":1},{"title":2}]')[1].title, 2);
assertEq(JSON.parse('[{"title":1},{"t\\u0069tle":2}]')[1].title, 2);
assertEq(JSON.parse('{"id":1,"id":2}').id, 2);
assertEq(JSON.parse('{"id":1}', function(key, value) {
    return key === 'id' ? JSON.parse('{"id":2}').id : value;
}).id, 2);

for (var bad of ['[{"title":1},{"title', '[{"title":1},{"title"',
                 '[{"title":1},{"titleX":}]', '[{"title":1},{"ti\ntle":2}]']) {
    var threw = false;
    try { JSON.parse(bad); } catch (e) { threw = e instanceof SyntaxError; }
    assertEq(threw, true);
}

// Keep cached atoms alive across allocations made while parsing later values.
var large = '[{"uncommonPropertyForGC":0},' +
            '{"other":[' + new Array(20000).fill('"allocation"').join(',') + ']},' +
            '{"uncommonPropertyForGC":1}]';
for (var i = 0; i < 3; i++) {
    gc();
    assertEq(JSON.parse(large)[2].uncommonPropertyForGC, 1);
}
