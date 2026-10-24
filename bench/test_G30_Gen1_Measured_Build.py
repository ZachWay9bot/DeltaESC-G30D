"""No hardware and no fake DRIVE acceptance: measured profile build-input checks."""
import importlib.util
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parent / "G30_Gen1_Measured_Build.py"
SPEC = importlib.util.spec_from_file_location("g30gen1build", SCRIPT)
MOD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MOD)

def candidate():
    return {
        "motor": "G30 Gen1",
        "measurement_evidence": "example-fixture-only-NOT-real-motor-data",
        "r_uohm": 120_000,
        "l_nh": 180_000,
        "flux_uwb": 10_000,
        "phase_offset_u16": -2500,
        "pole_pairs": 15,
        "wheel_circumference_mm": 760,
        "test_current_ma": 500,
    }

class MeasuredBuildTest(unittest.TestCase):
    def test_valid_fixture_generates_all_profile_overrides(self):
        c=candidate()
        self.assertIs(MOD.validate(c),c)
        f=MOD.make_profile_flags(c)
        for v in MOD.MACROS.values():
            self.assertIn("-D"+v+"=",f)
        self.assertIn("-DG30_PROFILE_POLE_PAIRS=15",f)
        self.assertIn("-DG30_PROFILE_PHASE_OFFSET_U16=-2500",f)

    def test_no_missing_measurements(self):
        for field in MOD.FIELDS:
            c=candidate();del c[field]
            with self.subTest(field=field),self.assertRaises(ValueError):
                MOD.validate(c)

    def test_no_donor_profile(self):
        c=candidate();c["motor"]="XBOT FO76"
        with self.assertRaises(ValueError):MOD.validate(c)

    def test_no_placeholder_or_invalid_numeric_data(self):
        for field in MOD.FIELDS:
            for v in (None, "100", 0, 2.5, True):
                c=candidate();c[field]=v
                with self.subTest(field=field,v=v),self.assertRaises(ValueError):
                    MOD.validate(c)

    def test_only_confirmed_pp(self):
        c=candidate();c["pole_pairs"]=6
        with self.assertRaises(ValueError):MOD.validate(c)

    def test_no_missing_evidence(self):
        c=candidate();c["measurement_evidence"]=""
        with self.assertRaises(ValueError):MOD.validate(c)

if __name__ == "__main__":
    unittest.main()
