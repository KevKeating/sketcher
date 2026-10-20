// CRDGEN-428 uses the replacement certified on 2026-10-20, without Orn.
export const bicyclicPeptide =
  'CHEM1{[CCC(N1CN(C(CC)=O)CN(C(CC*)=O)C1)=O |$_R1;;;;;;;;;;_R2;;;;;;;_R3;;;;$|]}|PEPTIDE1{A.R.D.C.P.L.V.N.P.L.C.L.H.P.G.W.T.C.[dV].C}$PEPTIDE1,CHEM1,4:R3-1:R1|PEPTIDE1,CHEM1,11:R3-1:R2|CHEM1,PEPTIDE1,1:R3-18:R3$$$V2.0';

// The three HELM strings in CRDGEN-434's certification instructions.
export const largeLabelPeptides = [
  'PEPTIDE1{[Me-].[Oic_3aR-7aS].[pnG].[dD]}|PEPTIDE2{[Me-].T.[dS].[Gly_cPr].C.[D-Nva].[D-2Nal].[meR].[D-Dip].[-OMe]}|PEPTIDE3{[D-2Thi].[Cys_Me].[dE].[Phe_4Me].G.[Hsl]}$PEPTIDE1,PEPTIDE3,4:R3-1:R1|PEPTIDE2,PEPTIDE1,5:R3-4:R2$$$V2.0',
  'PEPTIDE1{[3Abz].[hHis].C.[Hyp].[Tyr_Ph4OH].T.[Sta_3xi4xi].[Gly_cPr].[meM].[Wil].[Gly_allyl].[meR].[D-Dip].[D-xiIle]}|PEPTIDE2{[dE].[D-Nle].[meL]}$PEPTIDE1,PEPTIDE2,3:R3-1:R1$$$V2.0',
  'PEPTIDE1{A.[Phe_3Cl].[NMebAla].[Tyr_tBu].[D-aThr].E.[meQ].[His_1Bn].[dN].P.[App].[seC].[D-nTyr].D.[Phe_3F].[Hyp].[D-His_1Bn].[Thr_PO3H2].[Pyl].[dF]}|PEPTIDE2{A.[meA].[dE].[Cya].[D-Phe_4ureido].[Phe_2Me]}|PEPTIDE3{[aIle].[dH].[Lys_iPr]}$PEPTIDE1,PEPTIDE2,14:R3-1:R1|PEPTIDE1,PEPTIDE3,6:R3-1:R1$$$V2.0',
];
