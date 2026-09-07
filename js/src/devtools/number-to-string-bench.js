// Run with: js number-to-string-bench.js
// Also supports: xpcshell -f number-to-string-bench.js
// Compare identical optimized builds, alternating baseline and patched runs.
// Reports milliseconds (lower is better); this is not a browser-suite score.

(function() {
    "use strict";
    var iterations = 500000;
    var samples = 7;
    var checksum = 0;

    function convert(values, radix, count) {
        var total = 0;
        for (var i = 0; i < count; i++)
            total += values[i % values.length].toString(radix).length;
        return total;
    }

    function unique(count) {
        var total = 0;
        for (var i = 0; i < count; i++)
            total += (123456.125 + i).toString().length;
        return total;
    }

    function measure(name, run) {
        checksum += run(20000);
        var times = [];
        var expected = run(iterations);
        for (var sample = 0; sample < samples; sample++) {
            if (typeof gc === "function")
                gc();
            var start = Date.now();
            var result = run(iterations);
            times.push(Date.now() - start);
            if (result !== expected)
                throw new Error("inconsistent conversion: " + name);
            checksum += result;
        }
        times.sort(function(a, b) { return a - b; });
        print(name + ": median=" + times[3] + " ms; samples=" + times.join(","));
    }

    for (var size of [1, 2, 4, 5, 16]) {
        var values = [];
        for (var i = 0; i < size; i++)
            values.push(123456.125 + i);
        measure("decimal working set " + size, function(count) {
            return convert(values, 10, count);
        });
    }
    measure("unique decimals", unique);
    var integers = [123456, 654321, 123457, 654322];
    measure("integer working set 4", function(count) {
        return convert(integers, 10, count);
    });
    measure("hexadecimal working set 4", function(count) {
        return convert(integers, 16, count);
    });
    print("checksum=" + checksum);
})();
