// Exercise vector-sized spans, scalar tails, and mixed character encodings.
for (var length of [0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65]) {
    for (var needle of ["\0", "\x7f", "\x80", "\xff", "\u0100", "\ud800", "\uffff"]) {
        for (var position = 0; position <= length; ++position) {
            var text = "a".repeat(position) + needle + "a".repeat(length - position);
            for (var start of [0, position, position + 1, text.length]) {
                var expected = start <= position ? position : -1;
                assertEq(text.indexOf(needle, start), expected);
                assertEq(text.includes(needle, start), expected !== -1);
            }
            assertEq(text.indexOf(needle + "b"), -1);
            assertEq(text.indexOf(needle + "a"), position < length ? position : -1);
        }
    }
    var latin1 = "\0".repeat(length) + "\xff";
    assertEq(latin1.indexOf("\u0100"), -1); // Must not narrow to NUL on x86.
    assertEq(latin1.indexOf("\uffff"), -1); // Must not narrow to 0xff on x86.
    var wide = "\u0100" + latin1;
    assertEq(wide.indexOf("\xff"), length + 1);
    assertEq(wide.indexOf(latin1), 1);
}
