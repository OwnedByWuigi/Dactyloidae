#!/usr/bin/env perl
open(IN,  "<", "resource/rgb.txt");
open(OUT, ">", "src/color.c");

print(OUT "#include <Mw/Milsko.h>\n");
print(OUT "\n");
print(OUT "#include \"../external/stb_ds.h\"\n");
print(OUT "\n");
print(OUT "struct color {\n");
print(OUT "	char* key;\n");
print(OUT "	MwU32 value;\n");
print(OUT "};\n");
print(OUT "\n");
print(OUT "static struct color* colors = NULL;\n");
print(OUT "\n");
print(OUT "MwLLColor MwParseColorName(MwWidget handle, const char* color){\n");
print(OUT "	MwRGB rgb;\n");
print(OUT "	MwParseColorNameNoAllocate(color, &rgb);\n");
print(OUT
"	return MwLLAllocColor(handle->lowlevel, rgb.red, rgb.green, rgb.blue);\n"
);
print(OUT "}\n");
print(OUT "\n");
print(OUT "void MwParseColorNameNoAllocate(const char* color, MwRGB* rgb){\n");
print(OUT "	MwU32 v = shget(colors, color);");
print(OUT "	rgb->red = (v >> 16) & 0xff;");
print(OUT "	rgb->green = (v >> 8) & 0xff;");
print(OUT "	rgb->blue = (v >> 0) & 0xff;");
print(OUT "}\n");
print(OUT "\n");
print(OUT
      "static void add_color(const char* name, int red, int green, int blue){\n"
);
print(OUT "	MwU32 v = (red << 16) | (green << 8) | (blue << 0);\n");
print(OUT "	shput(colors, name, v);\n");
print(OUT "}\n");
print(OUT "\n");
print(OUT "void MwColorTableInit(void){\n");
print(OUT "	sh_new_strdup(colors);\n");
print(OUT "	shdefault(colors, 0);\n");

while (my $l = <IN>) {
    $l =~ s/\r?\n$//;
    if ($l =~ /^[ \t]*([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+(.+)$/) {
        print(OUT "	add_color(\"$4\", $1, $2, $3);\n");
    }
}
print(OUT "}\n");

close(OUT);
close(IN);
