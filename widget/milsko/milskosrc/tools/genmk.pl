#!/usr/bin/env perl
my @cfiles   = ();
my @examples = ();

sub scan {
    opendir(DIR, $_[0]);
    my @files = readdir(DIR);
    foreach my $f (@files) {
        if ($f =~ /^vulkan\.c$/) {
            next;
        }
        if ($f =~ /\.c$/) {
            push(@cfiles, $_[0] . "/" . $f);
        }
    }
    closedir(DIR);
}

sub scan_examples {
    opendir(DIR, $_[0]);
    my @files = readdir(DIR);
    foreach my $f (@files) {
        if ($f =~ /^glutlayer\.c$/) {
            next;
        }
        if ($f =~ /\.c$/) {
            push(@examples, $_[0] . "/" . $f);
        }
    }
    closedir(DIR);
}

sub cobjs {
    my $r = "";
    foreach my $f (@cfiles) {
        my $b = $f;
        $b =~ s/\.c$/.obj/;
        $b =~ s/\//$_[0]/g;

        $r = $r . " " . $_[1] . $b;
    }
    return $r;
}

sub generate {
    my ($output, $type) = @_;

    my $cc   = "";
    my $link = "";

    my $suffix     = 1;
    my $symbolic   = "";
    my $dir        = "\\";
    my $del        = "del /f /q";
    my $out        = "";
    my $dllout     = "";
    my $exeout     = "";
    my $def        = "";
    my $inc        = "";
    my $cdll       = "";
    my $dll        = "";
    my $exe        = "";
    my $prefobj    = "";
    my $needlibs   = "";
    my $lib        = "";
    my $c_dllout   = "";
    my $c_dllafter = "";

    if ($type eq "Borland") {
        $cc     = "bcc32 -c";
        $link   = "bcc32";
        $out    = "-o";
        $dllout = "-e";
        $exeout = "-e";
        $def    = "-D";
        $inc    = "-I";
        $dll    = "-tWD";
        $lib    = "-l";

        $c_dllafter = "implib src${dir}Mw.lib src${dir}Mw.dll";
    }
    elsif ($type eq "MSVC") {
        $cc     = "cl /TC /c /nologo";
        $link   = "link /nologo";
        $out    = "/Fo";
        $dllout = "/OUT:";
        $exeout = "/OUT:";
        $def    = "/D";
        $inc    = "/I";
        $dll    = "/DLL";
    }
    elsif ($type eq "Watcom") {
        $cc     = "wcc386 -bt=nt -q";
        $link   = "wlink option quiet";
        $out    = "-fo=";
        $dllout = "name ";
        $exeout = "name ";
        $def    = "-d";
        $inc    = "-i=";
        $cdll   = "-bd";
        $dll    = "system nt_dll";
        $exe    = "system nt";
        $lib    = "library ";

        $suffix   = 0;                  # watcom suffix rule works kinda strange
        $symbolic = ".SYMBOLIC";
        $dir      = "/";
        $del      = "%erase";
        $prefobj  = "file ";
        $needlibs = "${lib}clib3r.lib";
        $c_dllout = "option implib=src${dir}Mw.lib";
    }

    open(OUT, ">", $output);
    print(OUT "CC = $cc\n");
    print(OUT "LD = $link\n");
    print(OUT "\n");
    print(OUT
"MW_CFLAGS = ${cdll} ${inc}include ${inc}external${dir}libz${dir}include ${def}_MILSKO ${def}_MILSKO_BUILD ${def}USE_GDI ${def}USE_STB_IMAGE ${def}STBI_NO_SIMD ${def}USE_GDI_TEXT\n"
    );
    print(OUT "MW_LDFLAGS = $dll\n");
    print(OUT "EXE_CFLAGS = ${inc}include\n");
    print(OUT "EXE_LDFLAGS = $exe");
    print(OUT "\n");

    if ($suffix) {
        print(OUT ".SUFFIXES: .obj .c\n");
    }
    print(OUT "all: src${dir}Mw.dll");
    foreach my $f (@examples) {
        $b = $f;
        $b =~ s/\.c$/.exe/;
        $b =~ s/\//$dir/g;
        print(OUT " $b");
    }
    print(OUT "\n");
    print(OUT "lib: src${dir}Mw.dll\n");
    print(OUT "examples:");
    foreach my $f (@examples) {
        $b = $f;
        $b =~ s/\.c$/.exe/;
        $b =~ s/\//$dir/g;
        print(OUT " $b");
    }
    print(OUT "\n");
    print(OUT "clean: $symbolic\n");
    foreach my $f (@cfiles) {
        my $b = $f;
        $b =~ s/\.c$/.obj/;
        $b =~ s/\//$dir/g;
        print(OUT "	$del $b\n");
    }
    print(OUT "	$del src${dir}Mw.dll\n");
    print(OUT "	$del src${dir}Mw.lib\n");
    foreach my $f (@examples) {
        my $b = $f;
        $b =~ s/\.c$/.obj/;
        $b =~ s/\//$dir/g;
        print(OUT "	$del $b\n");
    }
    foreach my $f (@examples) {
        $b = $f;
        $b =~ s/\.c$/.exe/;
        $b =~ s/\//$dir/g;
        print(OUT "	$del $b\n");
    }
    print(OUT "\n");
    foreach my $f (@examples) {
        my $b = $f;
        $b =~ s/\.c$/.exe/;
        $b =~ s/\//$dir/g;
        my $o = $f;
        $o =~ s/\.c$/.obj/;
        $o =~ s/\//$dir/g;
        print(OUT "$b: $o src${dir}Mw.dll\n");
        print(OUT
"	\$(LD) \$(EXE_LDFLAGS) $exeout\$@ $prefobj$o $needlibs ${lib}src${dir}Mw.lib"
        );

        if ($f =~ /gldemos/) {
            print(OUT " ${lib}opengl32.lib ${lib}glu32.lib");
        }
        print(OUT "\n");
        print(OUT "\n");
        print(OUT "$o: $f\n");
        print(OUT "	\$(CC) \$(EXE_CFLAGS) ${out}\$@ $f\n");
        print(OUT "\n");
    }
    print(OUT "\n");
    print(OUT "src${dir}Mw.dll: " . cobjs($dir) . "\n");
    print(  OUT "	\$(LD) \$(MW_LDFLAGS) $c_dllout $dllout\$@ "
          . cobjs($dir, $prefobj)
          . " $needlibs ${lib}opengl32.lib ${lib}gdi32.lib ${lib}winmm.lib ${lib}user32.lib ${lib}advapi32.lib\n"
    );
    print(OUT "	$c_dllafter\n");
    print(OUT "\n");

    if ($suffix) {
        print(OUT ".c.obj:\n");
        print(OUT "	\$(CC) \$(MW_CFLAGS) ${out}\$@ \$<\n");
    }
    else {
        print(OUT "\n");
        foreach my $f (@cfiles) {
            my $o = $f;
            $o =~ s/\.c$/.obj/;
            print(OUT "$o: $f\n");
            print(OUT "	\$(CC) \$(MW_CFLAGS) ${out}\$@ $f\n");
        }
    }
    close(OUT);
}

scan("src");
scan("external");
scan("external/libz/src");
scan("src/icon");
scan("src/cursor");
scan("src/widget");
scan("src/text");
scan("src/text/font");
scan("src/dialog");
scan("src/abstract");
push(@cfiles, "src/backend/gdi.c");

@cfiles = sort(@cfiles);

scan_examples("examples/basic");
scan_examples("examples/gldemos");

@examples = sort(@examples);

generate("BorMakefile", "Borland");
generate("NTMakefile",  "MSVC");
generate("WatMakefile", "Watcom");
