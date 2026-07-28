#!/usr/bin/env perl

our $clean = 0;
my @objs = ();

sub compile {
    my ($source) = @_;
    my $object   = "";
    my $dir      = "";

    $dir = $source;
    $dir =~ s/\/([^\/]+)$//;

    $source =~ s/^(.+)\/([^\/]+)$/[\/\1]\2/;
    $source =~ s/\//\./g;

    $object = $source;
    $object =~ s/\.c$/.obj/;

    print(OUT "\$ if f\$search(\"$object;*\") .eqs. \"\"\n");
    print(OUT "\$ then\n");
    print(OUT "\$	write sys\$output \"CC $object\"\n");
    print(OUT
"\$	cc /include_directory=(\"./include\",\"./$dir\") /define=(_MILSKO,USE_X11,NO_IMAGE) /object=$object $source\n"
    );
    print(OUT "\$ endif\n");

    push(@objs, $object);
}

sub clean {
    my ($source) = @_;
    my $object   = "";
    my $dir      = "";

    $source =~ s/^(.+)\/([^\/]+)$/[\/\1]\2/;
    $source =~ s/\//\./g;

    $object = $source;
    $object =~ s/\.c$/.obj/;

    print(OUT
          "\$ if f\$search(\"$object\") .nes. \"\" then delete $object;*\n");
}

sub thing {
    if ($clean) {
        clean($_[0]);
    }
    else {
        compile($_[0]);
    }
}

sub scan {
    my ($dir) = @_;

    opendir(my $dh, $dir);
    while (my $file = readdir($dh)) {
        if (   !(($dir . "/" . $file) eq "src/widget/opengl.c")
            && !(($dir . "/" . $file) eq "src/widget/vulkan.c")
            && !(($dir . "/" . $file) eq "external/stb_truetype.c")
            && !(($dir . "/" . $file) eq "external/stb_image.c")
            && ($file =~ /\.c$/))
        {
            thing($dir . "/" . $file);
        }
    }
    closedir($dh);
}

for (my $i = 0 ; $i < 2 ; $i++) {
    $clean = $i;

    open(OUT, ">", $i == 0 ? "vms/build.com" : "vms/clean.com");
    scan("src");
    scan("src/cursor");
    scan("src/widget");
    scan("src/dialog");
    scan("src/font");
    scan("src/icon");
    scan("src/abstract");
    scan("external");
    thing("src/backend/x11.c");

    if ($i == 0) {
        print(OUT "\$ if f\$search(\"[.src]MwSHR.exe;*\") .eqs. \"\"\n");
        print(OUT "\$ then\n");
        print(OUT "\$	write sys\$output \"LINK [.src]MwSHR.exe\"\n");
        print(OUT "\$	OPEN /WRITE LINK_OPT LINK.OPT\n");
        foreach my $obj (@objs) {
            print(OUT "\$	WRITE LINK_OPT \"$obj\"\n");
        }
        print(OUT "\$	WRITE LINK_OPT \"SYS\$LIBRARY:DECW\$XLIBSHR/SHARE\"\n");
        print(OUT "\$	WRITE LINK_OPT \"SYS\$LIBRARY:DPML\$SHR/SHARE\"\n");
        print(OUT "\$	CLOSE LINK_OPT\n");
        print(OUT "\$	link /SHAREABLE=[.src]MwSHR.exe LINK.OPT/options\n");
        print(OUT "\$	DELETE LINK.OPT;*\n");
        print(OUT "\$ endif\n");
    }
    else {
        clean("src/MwSHR.exe");
    }
    close(OUT);
}
