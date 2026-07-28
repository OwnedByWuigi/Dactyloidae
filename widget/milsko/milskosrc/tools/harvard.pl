#!/usr/bin/env perl
open(IN, "<", "resource/harvard.txt");
print("const char* harvard[] = {\n");
while (my $l = <IN>) {
    $l =~ s/\r?\n$//;
    if ($l =~ /^[ \t]*[0-9]+\. (.+)$/) {
        print("	\"" . $1 . "\",\n");
    }
}
print("	NULL\n");
print("};\n");
close(IN);
