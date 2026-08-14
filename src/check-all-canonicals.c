// This software is part of github.com/waynebhayes/BLANT, and is Copyright(C) Wayne B. Hayes 2025, under the GNU LGPL 3.0
// (GNU Lesser General Public License, version 3, 2007), a copy of which is contained at the top of the repo.
//
// Read a BLANT canon_list file and run blant (L_K_Func_Sort) once on every
// canonical decimal it contains, printing the converted decimals.
//
// A canon_list file (as produced by the Makefile's canon_list%.txt rule, see
// fast-canon-map.c) has one leading line giving the number of canonicals N,
// followed by N lines whose FIRST field is a canonical decimal; the rest of
// each line is the connected/numedges/orbit annotation.
//
// NOTE on canonical definitions: the decimals stored in canon_list/canon_map
// are computed by fast-canon-map.c using the GLOBAL minimum over all k!
// labelings, whereas L_K_Func_Sort (used at runtime under DYNAMIC_CANON_MAP)
// uses the f(n)-sorted definition selected by CANON_ASCENDING_NEIGHBORS and
// SORT_CUBED_SUM in blant-fundamentals.h.  These two conventions can disagree
// on WHICH decimal is the canonical representative of a given isomorphism
// class (e.g. for k=3 the path {3,5,6} is canon 3 globally but canon 6 under
// the sorted definition).  That divergence is expected and accepted: running
// blant once on each file decimal converts it to blant's own canonical.
//
// No verification is performed; the program just converts.
//
// Build (not part of the normal build; only on explicit request):
//     make check-all-canonicals
// Usage:
//     ./check-all-canonicals k [directed] [canon_list_file]
// k            : number of nodes; undirected supports k in [2,8], directed k
//                in [2,6] (limited by the shipped canon_map sizes).
// "directed"   : (optional, as in fast-canon-map) read/graphete directed graphs.
// canon_list_file: (optional) explicit path to a canon_list file.  If omitted,
//                defaults to BLANT_CANON_DIR/canon_list{k}.txt (undirected) or
//                BLANT_CANON_DIR/directed/canon_list{k}.txt (directed), where
//                BLANT_CANON_DIR defaults to "canon_maps".
#include "blant.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int _k;      // normally defined in blant.c, which this harness does not link
Boolean _directed;    // ditto

// L_K_Func_Sort is defined in blant-utils.c but declared in no header under
// DYNAMIC_CANON_MAP (which is the only build mode used now).
Gint_type L_K_Func_Sort(Gint_type Gint, unsigned char permOut[], unsigned short olist[], Boolean computeOrbits);

// blant-utils.c calls PrintGintStderr/PrintGintStdout (from blant-output.c) when
// DEBUG_ATTEMPTS is on, and blant-utils.h turns it on unconditionally.
// blant-output.o is not linked into this harness, so provide stubs that use a
// fully general decimal printer (GINT_FMT is "%llu", which does NOT cover the
// 128-bit Gint_type used under DYNAMIC_CANON_MAP).
static void gintToStr(Gint_type v, char buf[], size_t cap) {
    char tmp[64];
    int i = 0;
    if (v == 0) { strcpy(buf, "0"); return; }
    while (v > 0) { tmp[i++] = '0' + (int)(v % 10); v /= 10; }
    size_t n = (size_t)i;
    if (n + 1 > cap) n = cap - 1;
    for (size_t j = 0; j < n; j++) buf[j] = tmp[n - 1 - j];
    buf[n] = '\0';
}
void PrintGintStderr(Gint_type Gint) {
    char buf[80];
    gintToStr(Gint, buf, sizeof(buf));
    fputs(buf, stderr);
}
void PrintGintStdout(Gint_type Gint) {
    char buf[80];
    gintToStr(Gint, buf, sizeof(buf));
    fputs(buf, stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 4) {
	fprintf(stderr, "USAGE: %s k [directed] [canon_list_file]\n", argv[0]);
	return -1;
    }
    int k = atoi(argv[1]);
    int argIdx = 2;
    _directed = false;
    if (argc > argIdx && strcmp(argv[argIdx], "directed") == 0) {
	_directed = true;
	argIdx++;
    }
    const char *explicitFile = (argc > argIdx) ? argv[argIdx] : NULL;

    if (_directed) {
	if (k < 2 || k > 6) {
	    fprintf(stderr, "directed graphs are only supported for k in [2,6]\n");
	    return -1;
	}
    } else if (k < 2 || k > 8) {
	fprintf(stderr, "undirected graphs are only supported for k in [2,8]\n");
	return -1;
    }
    _k = k;

    // Resolve the canon_list file path.
    char bufPath[1024];
    const char *path;
    if (explicitFile) {
	path = explicitFile;
    } else {
	if (_directed)
	    snprintf(bufPath, sizeof(bufPath), "%s/directed/canon_list%d.txt", DEFAULT_CANON_DIR, k);
	else
	    snprintf(bufPath, sizeof(bufPath), "%s/canon_list%d.txt", DEFAULT_CANON_DIR, k);
	path = bufPath;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
	fprintf(stderr, "cannot open canon_list file \"%s\"\n", path);
	return -1;
    }

    // First line is the count N.  Following lines: first field is a decimal.
    char line[4096];
    if (!fgets(line, sizeof(line), fp)) {
	fprintf(stderr, "empty canon_list file \"%s\"\n", path);
	fclose(fp);
	return -1;
    }
    long long N = strtoll(line, NULL, 10);
    if (N <= 0) {
	fprintf(stderr, "malformed first line of \"%s\" (expected a positive count, got \"%s\")", path, line);
	fclose(fp);
	return -1;
    }

    int bits = _directed ? k * (k - 1) : k * (k - 1) / 2;
    Gint_type total = ((Gint_type)1) << bits;

    Gint_type *dec = calloc((size_t)N, sizeof(Gint_type));
    if (!dec) { fprintf(stderr, "calloc(%lld) failed\n", N); fclose(fp); return -1; }

    long long nDec = 0;
    while (nDec < N && fgets(line, sizeof(line), fp)) {
	if (line[0] == '\n' || line[0] == '#') continue;
	dec[nDec] = (Gint_type)strtoll(line, NULL, 10);
	nDec++;
    }
    fclose(fp);

    fprintf(stderr, "converting %lld canonical decimals from \"%s\" (k=%d, %s, %d bits)...\n",
	N, path, k, _directed ? "directed" : "undirected", bits);

    // Print converted list: count line, then one converted decimal per line.
    fprintf(stdout, "%lld\n", nDec);
    for (long long i = 0; i < nDec; i++) {
	Gint_type d = dec[i];
	if (d >= total) {
	    fprintf(stderr, "WARNING: decimal "); PrintGintStderr(d);
	    fprintf(stderr, " out of range [0,%d bits)\n", bits);
	    continue;
	}
	unsigned char permOut[MAX_K];
	unsigned short olist[MAX_K];
	Gint_type canon = L_K_Func_Sort(d, permOut, olist, 0);
	PrintGintStdout(canon);
	fputc('\n', stdout);
    }

    free(dec);
    return 0;
}