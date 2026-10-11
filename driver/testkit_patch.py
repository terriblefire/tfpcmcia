"""Patch Amiga Test Kit for the tfpcmcia diagnostic ROM (driver/testkit.bin).

Adds: every printed line echoed to the serial port (9600 8N1), autostart
into Memory -> Test All Memory, and a simple chip/fast RAM speed test.

Rebuild testkit.bin:
    git clone https://github.com/keirf/amiga-stuff && cd amiga-stuff/testkit
    (LF line endings) python testkit_patch.py
    make TOOL_PREFIX=m68k-elf- VER=tfp AmigaTestKit.bin
    cp AmigaTestKit.bin <tfpcmcia>/driver/testkit.bin
Built here with m68k-neogeo-elf-gcc (any bare m68k-elf gcc works).
"""


def edit(p, pairs):
    s = open(p, newline="").read()
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:60], s.count(a))
        s = s.replace(a, b)
    open(p, "w", newline="").write(s)


edit("testkit.c", [
("void print_line(const struct char_row *r)\n{\n    struct menu_option _m, *m = NULL;\n",
 """/* tfpcmcia: echo every printed line to the serial port (9600 8N1). */
static void ser_putc(char c)
{
    static uint8_t ser_init;
    if (!ser_init) {
        cust->serper = div32(cpu_hz/2 + 4800, 9600) - 1;
        ser_init = 1;
    }
    while (!(cust->serdatr & (1u<<13)))
        continue;
    cust->serdat = 0x100 | (uint8_t)c;
}

void ser_puts(const char *s)
{
    for (; *s; s++)
        if (*s != '$')
            ser_putc(*s);
    ser_putc('\\r');
    ser_putc('\\n');
}

void print_line(const struct char_row *r)
{
    struct menu_option _m, *m = NULL;
"""),
("    char c;

    clear_text_rows(_y, 1);
",
 "    char c;

    ser_puts(r->s);
    clear_text_rows(_y, 1);
"),
("    while ((i = keycode_buffer - K_F1) >= ARRAY_SIZE(mainmenu_option)) {",
 """    /* tfpcmcia: go straight into the memory test on first entry. */
    {
        static uint8_t autostarted;
        if (!autostarted) {
            autostarted = 1;
            keycode_buffer = K_F1;
        }
    }

    while ((i = keycode_buffer - K_F1) >= ARRAY_SIZE(mainmenu_option)) {"""),
])

edit("memory.c", [
("static void kickstart_memory_test(struct char_row *r)\n",
 """void ser_puts(const char *s);

/* tfpcmcia: rudimentary RAM speed test. 128kB of move.l writes then reads
 * at the given address, timed with the CIA (cpu_hz/10 ticks per second). */
#define SPEED_BYTES 0x20000u
static unsigned int speed_kbps(uint32_t ticks)
{
    uint32_t ms = div32(ticks, div32(cpu_hz + 9999, 10000));
    if (ms == 0)
        ms = 1;
    return div32(SPEED_BYTES / 1024 * 1000, ms);
}

static void speed_test(const char *name, uint32_t addr,
                       unsigned int *wr, unsigned int *rd)
{
    volatile uint32_t *p = (volatile uint32_t *)addr;
    uint32_t t0, t1, i, sum = 0;
    char s[80];

    t0 = get_time();
    for (i = 0; i < SPEED_BYTES/4; i += 8) {
        p[i+0] = i; p[i+1] = i; p[i+2] = i; p[i+3] = i;
        p[i+4] = i; p[i+5] = i; p[i+6] = i; p[i+7] = i;
    }
    t1 = get_time();
    *wr = speed_kbps(t1 - t0);

    t0 = get_time();
    for (i = 0; i < SPEED_BYTES/4; i += 8)
        sum += p[i+0] + p[i+1] + p[i+2] + p[i+3]
            + p[i+4] + p[i+5] + p[i+6] + p[i+7];
    t1 = get_time();
    *rd = speed_kbps(t1 - t0);

    sprintf(s, "SPEED %s @%08x: write %u KB/s, read %u KB/s (sum %08x)",
            name, addr, *wr, *rd, sum);
    ser_puts(s);
}

static void mem_speed_test(struct char_row *r)
{
    char *s = (char *)r->s;
    unsigned int cw, cr, fw = 0, fr = 0, i;
    uint32_t a;

    /* Chip RAM above the 256kB testkit owns. */
    speed_test("chip", 0x40000, &cw, &cr);
    for (i = 0; i < nr_mem_regions; i++) {
        if (mem_region[i].attr & 2)
            continue;
        a = lb(mem_region[i].lower);
        if (ub(mem_region[i].upper) - a >= SPEED_BYTES)
            speed_test("fast", a, &fw, &fr);
    }
    sprintf(s, "Chip W/R %u/%u  Fast W/R %u/%u KB/s", cw, cr, fw, fr);
    r->x = 0;
    r->y = 13;
    print_line(r);
}

static void kickstart_memory_test(struct char_row *r)
"""),
("    next_key:\n        do {\n            while (!(key = keycode_buffer) && !do_exit)\n                continue;\n            keycode_buffer = 0;\n\n            if (do_exit || (key == K_ESC))\n                goto out;\n        } while ((key < K_F1) || (key > K_F6));\n",
 """    next_key:
        /* tfpcmcia: speed test, then straight into Test All Memory. */
        {
            static uint8_t autostarted;
            if (!autostarted) {
                autostarted = 1;
                mem_speed_test(&r);
                r.y = 12;
                keycode_buffer = K_F1;
            }
        }
        do {
            while (!(key = keycode_buffer) && !do_exit)
                continue;
            keycode_buffer = 0;

            if (do_exit || (key == K_ESC))
                goto out;
        } while ((key < K_F1) || (key > K_F6));
"""),
])
print("patched")
