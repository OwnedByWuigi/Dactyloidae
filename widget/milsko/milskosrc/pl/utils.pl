sub add_incdir {
    my $input = $_[0];
    $input =~ s/\r?\n/ /g;

    $incdir = "${incdir} ${input}";
}

sub add_incdir2 {
    my $input = $_[0];
    $input =~ s/\r?\n/ /g;

    $incdir2 = "${incdir2} ${input}";
}

sub add_cflags {
    my $input = $_[0];
    $input =~ s/\r?\n/ /g;

    $cflags = "${cflags} ${input}";
}

sub add_libdir {
    my $input = $_[0];
    $input =~ s/\r?\n/ /g;

    $libdir = "${libdir} ${input}";
}

sub add_ldflags {
    my $input = $_[0];
    $input =~ s/\r?\n/ /g;

    $ldflags = "${ldflags} ${input}";
}

sub add_libs {
    my $input = $_[0];
    $input =~ s/\r?\n/ /g;

    $libs = "${libs} ${input}";
}

sub new_example {
    if (@_ == 2) {
        $examples_libs{"$_[0]${executable_suffix}"} = $_[1];
    }
    push(@examples_targets, "${_[0]}${executable_suffix}");
}

sub set_shared_flag {
    @shared = $_[0];
}

sub new_object {
    my @l = glob($_[0]);
    foreach my $e (@l) {
        $e =~ s/\.(cc|c|m)$/$object_suffix/;
        push(@library_targets, $e);
    }
}

sub use_backend {
    foreach my $l (@_) {
        push(@backends, $l);
    }
}

sub scan_wayland_protocol_core {
    my $proto_c = "src/backend/wayland/wayland-core-protocol.c";

    if (
        system(
"wayland-scanner private-code /usr/share/wayland/wayland.xml ${proto_c}"
        ) != 0
      )
    {
        print(
"^ Error on getting private code for /usr/share/wayland/wayland.xml\n"
        );
    }
    else {
        new_object($proto_c);
    }
    if (
        system(
"wayland-scanner client-header /usr/share/wayland/wayland.xml include/Mw/LowLevel/Wayland/wayland-core-protocol.h"
        )
      )
    {
        print(
"^ Error on getting client header for /usr/share/wayland/wayland.xml\n"
        );
    }
}

sub scan_wayland_protocol {
    my $tier  = $_[0];
    my $proto = $_[1];
    my $ver   = $_[2];

    my $proto_c = "src/backend/wayland/wayland-${proto}-protocol.c";

    if (
        system(
"wayland-scanner private-code /usr/share/wayland-protocols/${tier}/${proto}/${proto}${ver}.xml ${proto_c}"
        ) != 0
      )
    {
        print(
"^ Error on getting private code for /usr/share/wayland-protocols/${tier}/${proto}/${proto}${ver}.xml\n"
        );
    }
    else {
        new_object($proto_c);
    }
    if (
        system(
"wayland-scanner client-header /usr/share/wayland-protocols/${tier}/${proto}/${proto}${ver}.xml include/Mw/LowLevel/Wayland/${proto}-client-protocol.h"
        )
      )
    {
        print(
"^ Error on getting client header for /usr/share/wayland-protocols/${tier}/${proto}/${proto}${ver}.xml\n"
        );
    }
}

sub scan_wayland_protocol_from_file {
    my $proto   = $_[0];
    my $file    = $_[1];
    my $proto_c = "src/backend/wayland/wayland-${proto}-protocol.c";

    if (
        system(
            "wayland-scanner private-code ./resource/wayland/${file} ${proto_c}"
        ) != 0
      )
    {
        print(
            "^ Error on getting private code for ./resource/wayland/${file}\n");
    }
    else {
        new_object($proto_c);
    }
    if (
        system(
"wayland-scanner client-header ./resource/wayland/${file} include/Mw/LowLevel/Wayland/${proto}-client-protocol.h"
        )
      )
    {
        print(
            "^ Error on getting client header for ./resource/wayland/${file}\n"
        );
    }
}

our %params = ();

sub param_set {
    $params{ $_[0] } = $_[1];
}

sub param_get {
    if (not(defined($params{ $_[0] }))) {
        return 0;
    }
    else {
        return $params{ $_[0] };
    }
}

sub param_list {
    return sort(keys(%params));
}

1;
