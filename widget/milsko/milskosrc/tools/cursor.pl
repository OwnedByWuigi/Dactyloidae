#!/usr/bin/env perl
my $font = $ARGV[0];
my $name = $ARGV[1];
my $out  = $ARGV[2];
my $char = "";
my $bmp  = 0;

my $w     = 0;
my $h     = 0;
my $ln    = 0;
my $com   = 0;
my $nocom = 0;

my $MAX = 16;

if ($name) {
    print("#include <Mw/Milsko.h>\n");
    print("\n");
}

open(IN, "<", $font) or die;
while (my $l = <IN>) {
    $l =~ s/\r?\n$//g;
    if (!$name && ($l =~ /^STARTCHAR ("?)(.+)\1$/)) {
        if (!($2 =~ /_mask$/)) {
            print("$2\n");
        }
    }
    elsif ($name && ($l =~ /^COPYRIGHT ("?)(.+)\1$/)) {
        if (!$com) {
            print("/**\n");
        }
        else {
            print(" *\n");
        }
        print(" * Copyright notice:\n");
        print(" *   $2\n");
        if (!$com) {
            print(" */\n");
        }
    }
    elsif ($name && ($l =~ /^COMMENT ("?)(.+)\1$/)) {
        if (!$nocom) {
            if (!$com) {
                $com = 1;
                print("/**\n");
            }
            print(" * $2\n");
        }
    }
    elsif ($name && ($l =~ /^ENDPROPERTIES$/)) {
        if ($com) {
            print(" */\n");
            $com   = 0;
            $nocom = 1;
        }
    }
    elsif (
        $name
        && (   ($l =~ /^STARTCHAR ("?)\Q$name\E\1$/)
            || ($l =~ /^STARTCHAR ("?)\Q${name}\E_(mask)\1$/))
      )
    {
        my $suffix = "";
        if ($2) {
            $suffix = "Mask";
        }
        $char = $name;
        print("MwCursor MwCursor$out$suffix = {\n");
    }
    elsif ($name
        && $char
        && ($l =~ /^BBX (\-?[0-9]+) (\-?[0-9]+) (\-?[0-9]+) (\-?[0-9]+)$/))
    {
        print("	$1, $2, $3, $4,\n");

        $w = $1 + 0;
        $h = $2 + 0;
    }
    elsif ($name && $char && ($l =~ /^BITMAP$/)) {
        print("	{\n");
        $bmp = 1;
        $ln  = 0;
    }
    elsif ($name && $char && ($l =~ /^ENDCHAR$/)) {
        $bmp  = 0;
        $char = "";
        for (my $i = 0 ; $i < ($MAX - $h) ; $i++) {
            if ($i == ($MAX - $h - 1)) {
                print("		0\n");
            }
            else {
                print("		0,\n");
            }
        }
        print("	}\n");
        print("};\n");
    }
    elsif ($bmp) {
        my $txt = "";

        my $c    = 0;
        my $n    = 0;
        my $incr = 0;

        my $num = 0;

        while ($c < $w) {
            if (($c % 8) == 0) {
                $n = hex("0x" . substr($l, $incr, 2));
                $incr += 2;
            }

            $num = $num << 1;
            if ($n & (1 << 7)) {
                $num = $num | 1;
                $txt = $txt . "#";
            }
            else {
                $num = $num | 0;
                $txt = $txt . ".";
            }

            $n = $n << 1;
            $c = $c + 1;
        }
        $ln = $ln + 1;
        if ($ln == $MAX) {
            print("		$num	/* $txt */\n");
        }
        else {
            print("		$num,	/* $txt */\n");
        }
    }
}
close(IN);
