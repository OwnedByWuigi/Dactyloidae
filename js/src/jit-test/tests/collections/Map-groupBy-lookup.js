// Repeated keys must reuse their group, including SameValueZero keys.
var objectKey = {};
var symbolKey = Symbol();
var keys = [undefined, null, false, 0, -0, NaN, "key", objectKey, symbolKey];
for (var iteration = 0; iteration < 100; iteration++) {
    var input = [];
    for (var repeat = 0; repeat < 4; repeat++) {
        for (var key of keys)
            input.push(key);
    }
    var calls = 0;
    var groups = Map.groupBy(input, function(value, index) {
        assertEq(index, calls++);
        return value;
    });
    assertEq(calls, input.length);
    assertEq(groups.size, 8);
    for (var key of keys)
        assertEq(groups.get(key).length, key === 0 ? 8 : 4);
    assertEq(groups.get(objectKey)[0], objectKey);
    assertEq(groups.get(symbolKey)[0], symbolKey);
    assertEq(groups.get(undefined)[0], undefined);
    var order = Array.from(groups.keys());
    assertEq(order[0], undefined);
    assertEq(order[3], 0);
    assertEq(order[4], NaN);
    assertEq(order[7], symbolKey);
}

// A throwing callback must still close the input iterator.
var closed = false;
function* values() {
    try {
        yield 1;
        yield 2;
    } finally {
        closed = true;
    }
}
var sentinel = {};
try {
    Map.groupBy(values(), function() { throw sentinel; });
    throw new Error("callback did not throw");
} catch (error) {
    assertEq(error, sentinel);
}
assertEq(closed, true);
