`large_label_monomers.json` supplies the 39 CRDGEN-434 certification symbols
absent from `chembl_monomers_no_id.json`. The tests read the ten matching
definitions directly from that existing fixture and merge them with this subset
in a temporary file. Default Sketcher definitions supply the standard residues.

Source: [Pistoia Alliance HELM Core Library](https://github.com/PistoiaHELM/HELMMonomerSets/blob/fecc972f4707bc4e0cc34b7de3713cdba559e177/HELMCoreLibrary/HELMCoreLibrary.json).
The subset preserves the original symbols, SMILES, names, natural analogs,
polymer/monomer types, and authors, using Sketcher's JSON field names. Unused
molfile, timestamp, ID, and redundant attachment-cap fields are omitted; the
mapped leaving atoms in each SMILES preserve attachment definitions.

The source is distributed under the accompanying
[MIT license](large_label_monomers.LICENSE).
