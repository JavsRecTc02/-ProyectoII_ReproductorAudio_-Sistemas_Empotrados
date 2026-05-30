/* Quartus Prime Version 18.1.0 Build 625 09/12/2018 SJ Lite Edition */
JedecChain;
	FileRevision(JESD32A);
	DefaultMfr(6E);

	P ActionCode(Ign)
		Device PartName(5CSEMA5F31) MfrSpec(OpMask(0) FullPath("C:/fpga/DE1-SoC-MyPlayer-master/soc_system.sof"));
	P ActionCode(Cfg)
		Device PartName(5CSEMA5F31) Path("C:/fpga/DE1-SoC-MyPlayer-master/") File("soc_system.sof") MfrSpec(OpMask(1));

ChainEnd;

AlteraBegin;
	ChainType(JTAG);
AlteraEnd;
