#!/usr/bin/perl -w
use strict;

### Identify instruction sets used in a binary file. ###

# Tavis Ormandy <taviso@gentoo.org> 2003
# Improvments by Will Woods <wwoods@gentoo.org>
# Perl convertion by Georgi Georgiev <chutz@gg3.net>
# Updated by Ryan Hill <dirtyepic.sk@gmail.com>
# Updated by Ward Poelmans <wpoely86@gmail.com> 2009

# initialize everything to zero.
my ($i486,$i586,$ppro,$mmx,$sse,$sse2,$sse3,$sse41,$sse42,$sse4a,$amd,$amd2,$cpuid) = (0,0,0,0,0,0,0,0,0,0,0,0);
my ($vendor, $subarch);

# unfortunately there are mnemonic collisions between vendor sets
# so check vendor_id string, and enable relevant sets.
print "Checking vendor_id string... ";
my $param = $ARGV[0];
unless (defined $ARGV[1] and $ARGV[0] eq "--vendor") {
	open FLAGS, "grep -Em1 '^flags' /proc/cpuinfo | " or die "could not read cpu flags in $!\n";
	#while ( $flags 
	my @flags=<FLAGS>;
	close FLAGS;
	
	open PIPE, "grep -Em1 '^vendor_id.*: ' /proc/cpuinfo | cut -d' ' -f2 | " or die "could not read vendor_id";
	$_ = <PIPE>;
	close PIPE;

	$param = $ARGV[0];
	if (/GenuineIntel/)		{ $vendor="intel";		print "GenuineIntel\n" }
	elsif (/AuthenticAMD/)	{ $vendor="amd"; }
	elsif (/CyrixInstead/)	{ $vendor="cyrix";		print "CyrixInstead\n" }
	elsif (/GenuineTMx86/)	{ $vendor="transmeta";	print "GenuineTMx86\n" }
	else					{ $vendor="other";		print "other\n" }

	if ($vendor eq "amd") {
		foreach ( @flags ) {
			if (/sse2/) { $vendor="amd64"; }
		}
	}
	if ($vendor eq "amd64")		{ print "AuthenticAMD 64\n"; }
	elsif ($vendor eq "amd")	{ print "AuthenticAMD\n"; }
	}

else {
	($vendor) = $ARGV[1];
	printf "%s\n", $vendor;
	$param = $ARGV[2];
}

# quick sanity tests.
defined $param	or die "usage: $0 [--vendor=intel|amd|amd64|cyrix|transmeta] /path/to/binary\n";
-e $param or die "error: $param does not exist.\n";
-r $param or die "error: cant read $param.\n";

printf "Disassembling %s, please wait...\n", $param;

# initialize screen output
if ($vendor eq "intel") {
	printf "i486: %4u i586: %4u ppro: %4u mmx: %4u sse: %4u sse2: %4u sse3: %4u sse4.1: %4u sse4.2: %4u\r",
		$i486, $i586, $ppro, $mmx, $sse, $sse2, $sse3, $sse41, $sse42;
}
elsif ($vendor eq "amd") {
	printf "i486: %4u i586: %4u mmx: %4u sse: %4u 3dnow: %4u ext3dnow: %4u\r",
		$i486, $i586, $mmx, $sse, $amd, $amd2;
}
elsif ($vendor eq "amd64") {
	printf "i486: %4u i586: %4u ppro: %4u mmx: %4u 3dnow: %4u ext3dnow: %4u sse: %4u sse2: %4u sse3: %4u sse4a: %4u\r",
		$i486, $i586, $ppro, $mmx, $amd, $amd2, $sse, $sse2, $sse3, $sse4a;
}
elsif ($vendor eq "cyrix") {
	printf "i486: %4u i586: %4u mmx: %4u\r",
		$i486, $i586, $mmx;
}
elsif ($vendor eq "transmeta") {
	printf "i486: %4u i586: %4u mmx: %4u\r",
		$i486, $i586, $mmx;
}
else {
	printf "i486: %4u i586: %4u ppro: %4u mmx: %4u sse: %4u sse2: %4u sse3: %4u sse4.1: %4u sse4.2: %4u\r",
		$i486, $i586, $ppro, $mmx, $sse, $sse2, $sse3, $sse41, $sse42;
}

# do the disassembling.
#
# see binutils src include/opcode/* --de.

open PIPE, "objdump -d $param | cut -f3 | cut -d' ' -f1 |" or die;
my $print;
while (defined (my $instruction = <PIPE>)) {
	chomp $instruction; 
	# 486
	if (scalar (grep /^$instruction$/, "bswap","cmpxchg","invd","invlpg","wbinvd","xadd")) { $i486++; $print=1 }
	# 586
	elsif (grep /^$instruction$/, "cmpxchg8b","rdmsr","rdtsc","wrmsr") { $i586++; $print=1 }
	# Pentium Pro
	elsif (grep /^$instruction$/, "cmova","cmovae","cmovb","cmovbe","cmovc","cmove","cmovg","cmovge","cmovl","cmovle","cmovna","cmovnae","cmovnb","cmovnbe","cmovnc","cmovne","cmovng","cmovnge","cmovnl","cmovnle","cmovno","cmovnp","cmovns","cmovnz","cmovo","cmovp","cmovs","cmovz","fcmova","fcmovae","fcmovb","fcmovbe","fcmove","fcmovna","fcmovnae","fcmovnb","fcmovnbe","fcmovne","fcmovnu","fcmovu","fcomi","fcomip","fcompi","fucomi","fucomip","fucompi","fxrstor","fxsave","rdpmc","sysenter","sysexit","ud2","ud2a","ud2b") { $ppro++; $print=1 }
	# MMX
	elsif (grep /^$instruction$/, "emms","movd","movq","packssdw","packsswb","packuswb","paddb","paddd","paddsb","paddsw","paddusb","paddusw","paddw","pand","pandn","pcmpeqb","pcmpeqd","pcmpeqw","pcmpgtb","pcmpgtd","pcmpgtw","pmaddwd","pmulhw","pmullw","por","pslld","psllq","psllw","psrad","psraw","psrld","psrlq","psrlw","psubb","psubd","psubsb","psubsw","psubusb","psubusw","psubw","punpckhbw","punpckhdq","punpckhwd","punpcklbw","punpckldq","punpcklwd","pxor") { $mmx++; $print=1}
	# SSE
	elsif (grep /^$instruction$/, "addps","addss","andnps","andps","cmpeqps","cmpeqss","cmpleps","cmpless","cmpltps","cmpltss","cmpneqps","cmpneqss","cmpnleps","cmpnless","cmpnltps","cmpnltss","cmpordps","cmpordss","cmpps","cmpss","cmpunordps","cmpunordss","comiss","cvtpi2ps","cvtps2pi","cvtsi2ss","cvtss2si","cvttps2pi","cvttss2si","divps","divss","ldmxcsr","maskmovq","maxps","maxss","minps","minss","movaps","movhlps","movhps","movlhps","movlps","movmskps","movntps","movntq","movss","movups","mulps","mulss","orps","pavgb","pavgw","pextrw","pinsrw","pmaxsw","pmaxub","pminsw","pminub","pmovmskb","pmulhuw","prefetchnta","prefetcht0","prefetcht1","prefetcht2","psadbw","pshufw","rcpps","rcpss","rsqrtps","rsqrtss","sfence","shufps","sqrtps","sqrtss","stmxcsr","subps","subss","ucomiss","unpckhps","unpcklps","xorps") { $sse++; $print=1 }
	# SSE2
	elsif (grep /^$instruction$/, "addpd","addsd","andnpd","andpd","clflush","cmpeqpd","cmpeqsd","cmplepd","cmplesd","cmpltpd","cmpltsd","cmpneqpd","cmpneqsd","cmpnlepd","cmpnlesd","cmpnltpd","cmpnltsd","cmpordpd","cmpordsd","cmppd","cmpsd","cmpunordpd","cmpunordsd","comisd","cvtdq2pd","cvtdq2ps","cvtpd2dq","cvtpd2pi","cvtpd2ps","cvtpi2pd","cvtps2dq","cvtps2pd","cvtsd2si","cvtsd2ss","cvtsi2sd","cvtss2sd","cvttpd2dq","cvttpd2pi","cvttps2dq","cvttsd2si","divpd","divsd","lfence","maskmovdqu","maxpd","maxsd","mfence","minpd","minsd","movapd","movd","movdq2q","movdqa","movdqu","movhpd","movlpd","movmskpd","movntdq","movnti","movntpd","movq","movq2dq","movsd","movupd","mulpd","mulsd","orpd","packssdw","packsswb","packuswb","paddb","paddd","paddq","paddsb","paddsw","paddusb","paddusw","paddw","pand","pandn","pause","pavgb","pavgw","pcmpeqb","pcmpeqd","pcmpeqw","pcmpgtb","pcmpgtd","pcmpgtw","pextrw","pinsrw","pmaddwd","pmaxsw","pmaxub","pminsw","pminub","pmovmskb","pmulhuw","pmulhw","pmullw","pmuludq","por","psadbw","pshufd","pshufhw","pshuflw","pslld","pslldq","psllq","psllw","psrad","psraw","psrld","psrldq","psrlq","psrlw","psubb","psubd","psubq","psubsb","psubsw","psubusb","psubusw","psubw","punpckhbw","punpckhdq","punpckhqdq","punpckhwd","punpcklbw","punpckldq") { $sse2++; $print=1 }
	# 3DNow
	elsif (grep /^$instruction$/, "pavgusb","pfadd","pfsub","pfsubr","pfacc","pfcmpge","pfcmpgt","pfcmpeq","pfmin","pfmax","pi2fw","pi2fd","pf2iw","pf2id","pfrcp","pfrsqrt","pfmul","pfrcpit1","pfrsqit1","pfrcpit2","pmulhrw","pswapw","femms","prefetch") { $amd++; $print=1 }
	# Ext3DNow
	elsif (grep /^$instruction$/, "pf2iw","pfnacc","pfpnacc","pi2fw","pswapd","maskmovq","movntq","pavgb","pavgw","pextrw","pinsrw","pmaxsw","pmaxub","pminsw","pminub","pmovmskb","pmulhuw","prefetchnta","prefetcht0","prefetcht1","prefetcht2","psadbw","pshufw","sfence") { $amd2++; $print=1 }
	# SSE3
	elsif (grep /^$instruction$/, "addsubpd","addsubps","fisttp","fisttpl","fisttpll","haddpd","haddps","hsubpd","hsubps","lddqu","monitor","movddup","movshdup","movsldup","mwait") { $sse3++; $print=1 }
	# SSE4.1
	elsif (grep /^$instruction$/, "mpsadbw", "phminposuw", "pmuldq", "pmulld", "dpps", "dppd", "blendps", "blendpd", "blendvps", "blendvpd", "pblendvb", "pblendw", "pminsb", "pmaxsb", "pminuw", "pmaxuw", "pminud", "pmaxud", "pminsd", "pmaxsd", "roundps", "roundss", "roundpd", "roundsd", "insertps", "pinsrb", "pinsrd", "pinsrq", "extractps", "pextrb", "pextrw", "pextrd", "pextrq", "pmovsxbw", "pmovzxbw", "pmovsxbd", "pmovzxbd", "pmovsxbq", "pmovzxbq", "pmovsxwd", "pmovzxwd", "pmovsxwq", "pmovzxwq", "pmovsxdq", "pmovzxdq", "movntdqa", "packusdw", "pcmpeqq", "ptest") { $sse41++; $print=1 }
	# SSE4.2
	elsif (grep /^$instruction$/, "crc32", "pcmpestri", "pcmpestrm", "pcmpistri", "pcmpistrm", "pcmpgtq", "popcnt") { $sse42++; $print=1 }
	# SSE4a
	elsif (grep /^$instruction$/, "lzcnt", "popcnt", "extrq", "insertq", "movntsd" ,"movntss") { $sse4a++; $print=1 }
	# CpuID
	elsif (grep /^$instruction$/, "cpuid") {$cpuid++, $i586++; $print=1 }
	
	if ($print) {
		if ($vendor eq "intel") {
			printf "i486: %4u i586: %4u ppro: %4u mmx: %4u sse: %4u sse2: %4u sse3: %4u sse4.1: %4u sse4.2: %4u\r",
				$i486, $i586, $ppro, $mmx, $sse, $sse2, $sse3, $sse41, $sse42;
		} elsif ($vendor eq "amd") {
			printf "i486: %4u i586: %4u mmx: %4u sse: %4u 3dnow: %4u ext3dnow: %4u\r",
				$i486, $i586, $mmx, $sse, $amd, $amd2;
		} elsif ($vendor eq "amd64") {
			printf "i486: %4u i586: %4u ppro: %4u mmx: %4u 3dnow: %4u ext3dnow: %4u sse: %4u sse2: %4u sse3: %4u sse4a: %4u\r",
				$i486, $i586, $ppro, $mmx, $amd, $amd2, $sse, $sse2, $sse3, $sse4a;
		} elsif ($vendor eq "cyrix") {
			printf "i486: %4u i586: %4u mmx: %4u\r",
				$i486, $i586, $mmx;
		} elsif ($vendor eq "transmeta") {
			printf "i486: %4u i586: %4u mmx: %4u\r",
				$i486, $i586, $mmx;
		} else {
			printf "i486: %4u i586: %4u ppro: %4u mmx: %4u sse: %4u sse2: %4u sse3: %4u sse4.1: %4u sse4.2: %4u\r",
				$i486, $i586, $ppro, $mmx, $sse, $sse2, $sse3, $sse41, $sse42;
		}
		undef $print;
	}
}

# print a newline
print "\n";

# cpuid instruction could mean the application checks to see
# if an instruction is supported before executing it. This might 
# mean it will work on anything over a pentium.
if ($cpuid) {
	printf "\nThis binary was found to contain the cpuid instruction.\n";
	printf "It may be able to conditionally execute instructions if\n";
	printf "they are supported on the host (i586+).\n\n";
}

# print minimum required processor, if there are collissions
# use the vendor to decide what to print.
if ($sse42) {
	if ($vendor eq "intel") {
		$subarch="Intel Core i5/i7 (Nehalem) w/ SSE4.2"
	}
 } elsif ($sse41) {
	if ($vendor eq "intel") {
		$subarch="Intel Core 2 Duo (Penryn) w/ SSE4.1"
	}
 } elsif ($sse4a) {
	if ($vendor eq "amd64") {
		$subarch="AMD Phenom (Barcelona) w/ SSE4a"
	}
 } elsif ($sse3) {
	if ($vendor eq "intel") {
		$subarch="Pentium IV (pentium4) w/ SSE3"
	} elsif ($vendor eq "amd64") {
		$subarch="AMD Athlon64 w/ SSE3"
	}
} elsif ($sse2) {
	if ($vendor eq "intel") {
		$subarch="Pentium IV (pentium4)"
	} elsif ($vendor eq "amd64") {
		$subarch="AMD Athlon64"
	}
} elsif ($sse) {
	if ($vendor eq "intel") {
		$subarch="Pentium III (pentium3)"
	} elsif ($vendor eq "amd") {
		$subarch="AMD Athlon 4 (athlon-4)"
	} else {
		$subarch="Pentium III (pentium3)"
	}
} elsif ($vendor eq "amd" and $amd2) {
	$subarch="AMD Athlon (athlon)"
} elsif ($vendor eq "amd" and $amd) {
	$subarch="AMD K6 III (k6-3)"
} elsif ($mmx) {
	if ($vendor eq "intel") {
		if ($ppro) {
			$subarch="Pentium II (pentium2)"
		} else {
			$subarch="Intel Pentium MMX [P55C] (pentium-mmx)"
		}
	} elsif ($vendor eq "amd") {
		$subarch="AMD K6 (k6)"
	} elsif ($vendor eq "cyrix") {
		$subarch="Cyrix 6x86MX / MII (pentium-mmx)"
	} else {
		$subarch="Intel Pentium MMX [P55C] (pentium-mmx)"
	}
} elsif ($ppro) {
	$subarch="Pentium Pro (i686 or pentiumpro)"
} elsif ($i586) {
	$subarch="Pentium or compatible (i586 or pentium)"
} elsif ($i486) {
	$subarch="80486 or comaptible (i486)"
} else {
	$subarch="80386 or compatible (i386)"
}

# print message and exit.
printf "%s will run on %s or higher processor.\n", $param, $subarch;
