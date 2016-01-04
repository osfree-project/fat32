#
# Macros for IPF/INF
#

IPFC    = wipfc

.SUFFIXES:
.SUFFIXES: .inf .ipf .bmp

all: $(TARGETS) .SYMBOLIC

.ipf.inf: .AUTODEPEND
 $(IPFC) -i $< -o $^@
