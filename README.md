# CIS 5470 Final Project
Christopher Liu (liuchris) and Connor Cummings (connorcc)

## Double Free (CWE-415) Analysis
Double free detection was done using dataflow and pointer analysis.

### Handwritten Tests
Simple handwritten tests are located in the `/test` directory.

* `test01.c` - Basic double free (should warn)
* `test02.c` - No double free (should NOT warn)
* `test03.c` - Double free inside a conditional branch (should warn)
* `test04.c` - Double free across mutually exclusive branches (should warn)
* `test05.c` - Interprocedural double free via a helper function (should warn)
* `test06.c` - Double free through aliasing (should warn)
* `test07.c` - Double free due to pointer reassignment across branches (should warn)
* `test08.c` - Freed pointer overwritten with NULL (should NOT warn)

The tests can be run using `make`:
```bash
$ cd test
$ make all
```

The results are stored in `.out` files in the test directory.

### Juliet Testbench
The Juliet Test Suite is a large collection of synthetic C/C++ programs created by NIST to evaluate
static and dynamic analysis tools. Each file contains one or more functions labeled as good (safe)
or bad (contains a known vulnerability), allowing tools to be tested for true positives, false
positives, and false negatives.

For CWE-415, there are 1266 test cases, which can be run using `make` and evaluated using a Python
script:
```bash
$ cd juliet/CWE415_Double_Free
$ make all
$ python3 eval.py
```

The current results are as follows (n = 1266, with 496 bad programs and 770 good programs):

| Metric | Value |
| ------ | ----- |
| True Positives (bad programs, rejected) | 150 |
| False Negatives (bad programs, accepted) | 346 |
| False Positives (good programs, rejected) | 12 |
| True Negatives (good programs, accepted) | 758 |
| **Precision** | **0.926** |
| **Recall** | **0.302** |
| **F-Score** | **0.455** |

## Use-After-Free (CWE-416) Analysis
Use-after-free detection was done using dataflow and pointer analysis, similar to double free
analysis.

### Handwritten Tests
Simple handwritten tests are located in the `/test` directory.

* `test01.c` - Basic UAF from a load instruction (should warn)
* `test02.c` - Basic UAF from a store instruction (should warn)
* `test03.c` - UAF with aliasing (should warn)
* `test04.c` - UAF inside a call with freed pointer as an argument (should warn)
* `test05.c` - Free null pointer and no dereference, no UAF (should NOT warn)
* `test06.c` - Possible UAF via branch condition (should warn)

The tests can be run using `make`:
```bash
$ cd test
$ make all
```

The results are stored in `.out` files in the test directory.

### Juliet Testbench
For CWE-416, there are 312 test cases, which can be run using `make` and evaluated using a Python
script:
```bash
$ cd juliet/CWE416_Use_After_Free
$ make all
$ python3 eval.py
```

The current results are as follows (n = 312, with 150 bad programs and 162 good programs):

| Metric | Value |
| ------ | ----- |
| True Positives (bad programs, rejected) | 150 |
| False Negatives (bad programs, accepted) | 0 |
| False Positives (good programs, rejected) | 24 |
| True Negatives (good programs, accepted) | 138 |
| **Precision** | **0.862** |
| **Recall** | **1.000** |
| **F-Score** | **0.925** |
