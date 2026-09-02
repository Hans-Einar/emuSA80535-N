from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count == 0 and new in text:
        return
    if count != 1:
        raise SystemExit("{}: expected one anchor, got {}".format(path, count))
    p.write_text(text.replace(old, new, 1))


replace_once(
    "emu8051.h",
    """enum em8051_timer
{
    EM8051_TIMER0 = 0,
    EM8051_TIMER1,
    EM8051_TIMER_COUNT
};""",
    """enum em8051_timer
{
    EM8051_TIMER0 = 0,
    EM8051_TIMER1,
    EM8051_TIMER2,
    EM8051_TIMER_COUNT
};""")

replace_once(
    "emu8051.h",
    """enum SAB_T2CON_MASKS
{
    SAB_T2CONMASK_I2FR = 0x20,
    SAB_T2CONMASK_I3FR = 0x40
};""",
    """enum SAB_T2CON_MASKS
{
    SAB_T2CONMASK_T2I0 = 0x01,
    SAB_T2CONMASK_T2I1 = 0x02,
    SAB_T2CONMASK_T2CM = 0x04,
    SAB_T2CONMASK_T2R0 = 0x08,
    SAB_T2CONMASK_T2R1 = 0x10,
    SAB_T2CONMASK_I2FR = 0x20,
    SAB_T2CONMASK_I3FR = 0x40,
    SAB_T2CONMASK_T2PS = 0x80
};""")

replace_once(
    "core.c",
    """static void sab_external_maintain_level_requests(struct em8051 *aCPU);
static void sab_adc_tick(struct em8051 *aCPU);""",
    """static void sab_external_maintain_level_requests(struct em8051 *aCPU);
static void sab_adc_tick(struct em8051 *aCPU);
static void sab_timer2_tick(struct em8051 *aCPU);""")

replace_once(
    "core.c",
    """    if (aTimer == EM8051_TIMER0)
    {
        record.tl = aCPU->mSFR[REG_TL0];
        record.th = aCPU->mSFR[REG_TH0];
    }
    else
    {
        record.tl = aCPU->mSFR[REG_TL1];
        record.th = aCPU->mSFR[REG_TH1];
    }""",
    """    if (aTimer == EM8051_TIMER0)
    {
        record.tl = aCPU->mSFR[REG_TL0];
        record.th = aCPU->mSFR[REG_TH0];
    }
    else if (aTimer == EM8051_TIMER1)
    {
        record.tl = aCPU->mSFR[REG_TL1];
        record.th = aCPU->mSFR[REG_TH1];
    }
    else
    {
        record.tl = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_TL2)];
        record.th = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_TH2)];
    }""")

replace_once(
    "core.c",
    """    // TODO: serial port, timer2, other stuff
}

static bool sab_irq_timer2_is_enabled(const struct em8051 *aCPU)""",
    """    // TODO: serial port, other stuff
}

static void sab_timer2_tick(struct em8051 *aCPU)
{
    uint8_t t2con;
    uint16_t value;
    uint64_t completed_cycle;

    if (!aCPU || aCPU->mVariant != EM8051_VARIANT_SAB80535)
        return;

    t2con = aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_T2CON)];
    if ((t2con & (SAB_T2CONMASK_T2I1 | SAB_T2CONMASK_T2I0)) !=
        SAB_T2CONMASK_T2I0)
    {
        return;
    }

    /* SLC-015 deliberately leaves hardware reload modes producer-inert. */
    if (t2con & SAB_T2CONMASK_T2R1)
        return;

    completed_cycle = aCPU->mMachineCycleCount + 1u;
    if ((t2con & SAB_T2CONMASK_T2PS) &&
        ((completed_cycle & 1u) != 0u))
    {
        return;
    }

    value = (uint16_t)(
        ((uint16_t)aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_TH2)] << 8) |
        aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_TL2)]);
    value = (uint16_t)(value + 1u);
    aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_TL2)] =
        (uint8_t)(value & 0xffu);
    aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_TH2)] =
        (uint8_t)(value >> 8);

    if (value == 0u)
    {
        aCPU->mSFR[SAB_SFR_INDEX(EM8051_SAB_SFR_IRCON)] |=
            SAB_IRCONMASK_TF2;
        timer_overflow_emit(aCPU, EM8051_TIMER2);
        sab_irq_sync(aCPU);
    }
}

static bool sab_irq_timer2_is_enabled(const struct em8051 *aCPU)""")

replace_once(
    "core.c",
    """static void advance_machine_cycle(struct em8051 *aCPU)
{
    timer_tick(aCPU);
    sab_adc_tick(aCPU);""",
    """static void advance_machine_cycle(struct em8051 *aCPU)
{
    timer_tick(aCPU);
    sab_timer2_tick(aCPU);
    sab_adc_tick(aCPU);""")

mk = Path("tests/Makefile")
text = mk.read_text()
replacements = [
    ("ADC_BIN := stage3_adc_tests.exe\nDEBUG_BIN_TEST := debug_facade_tests.exe",
     "ADC_BIN := stage3_adc_tests.exe\nTIMER2_BIN := stage4_timer2_tests.exe\nDEBUG_BIN_TEST := debug_facade_tests.exe"),
    ("ADC_BIN := stage3_adc_tests\nDEBUG_BIN_TEST := debug_facade_tests",
     "ADC_BIN := stage3_adc_tests\nTIMER2_BIN := stage4_timer2_tests\nDEBUG_BIN_TEST := debug_facade_tests"),
    ("'stage3_adc_tests.exe','stage3_adc_tests','debug_facade_tests.exe'",
     "'stage3_adc_tests.exe','stage3_adc_tests','stage4_timer2_tests.exe','stage4_timer2_tests','debug_facade_tests.exe'"),
    ("'stage3_adc_tests.ilk','stage3_adc_tests.pdb','debug_facade_tests.ilk'",
     "'stage3_adc_tests.ilk','stage3_adc_tests.pdb','stage4_timer2_tests.ilk','stage4_timer2_tests.pdb','debug_facade_tests.ilk'"),
    ("stage3_adc_tests stage3_adc_tests.exe debug_facade_tests",
     "stage3_adc_tests stage3_adc_tests.exe stage4_timer2_tests stage4_timer2_tests.exe debug_facade_tests"),
    ("all: $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(EDGE_BIN) $(ADC_BIN)",
     "all: $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(EDGE_BIN) $(ADC_BIN) $(TIMER2_BIN)"),
    ("$(ADC_BIN): test_stage3_adc.c $(CORE_SRC) ../emu8051.h\n\t$(CC) $(CFLAGS) -o $@ test_stage3_adc.c $(CORE_SRC)\n",
     "$(ADC_BIN): test_stage3_adc.c $(CORE_SRC) ../emu8051.h\n\t$(CC) $(CFLAGS) -o $@ test_stage3_adc.c $(CORE_SRC)\n\n$(TIMER2_BIN): test_stage4_timer2.c $(CORE_SRC) ../emu8051.h\n\t$(CC) $(CFLAGS) -o $@ test_stage4_timer2.c $(CORE_SRC)\n"),
    ("test: $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(EDGE_BIN) $(ADC_BIN)",
     "test: $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(EDGE_BIN) $(ADC_BIN) $(TIMER2_BIN)"),
    ("\t./$(ADC_BIN)\n\n",
     "\t./$(ADC_BIN)\n\t./$(TIMER2_BIN)\n\n"),
]
for old, new in replacements:
    count = text.count(old)
    if count == 0 and new in text:
        continue
    if count != 1:
        raise SystemExit("tests/Makefile anchor expected once, got {}".format(count))
    text = text.replace(old, new, 1)
mk.write_text(text)
