We need to implement new side bar buttons that pop up a widget that allows the user to search the monomer database.  Selecting a monomer in this widget will update the SketcherModel in order switch the scene tool to a DrawMonomerSceneTool using the selected monomer.

## General information
- The Jira ID for this case is SKETCH-2827.  I've downloaded the image attachments for that case as well as the Google Doc of the spec and placed them in `F:\Documents\sketcher\SKETCH-2827`.  In `Dual Mode Sketcher v1 - design updates.docx`, we're implementing the interface described in the "Longer term approach:" subsection of the "Using the Monomer DB" section.
- Note that there has been some additional offline discussion regarding the spec.  If any of my instructions deviate from the spec, it's probably as a result of this offline discussion, but you should double check with me to be sure.
- There are two components of the spec that we're not implementing yet: the "Matching SMILES" filter and the "Recently used items" component.  We still want to honor the "State of popup should probably be maintained, so that opening the popup automatically loads the last-used item" request from the "Recently used items:" section of the spec, though.
- There will be two Monomer Database buttons on the side bar: one in amino_page of MonomerToolWidget and one in nucleic_page.  The amino_page button pop up allows the user to select the peptide or chem polymer types, while the nucleic_page button pop up allows the user to select RNA or chem polymer types.
- These new buttons are going to replace the "Unclassified" buttons added in 5a9c9405400cd0374511bfdcf1a0e85ff24a1917 and also dramatically simplify the monomer button pop ups.  I've given additional information about this below.

## The overall plan

This change is quite large, so I've broken the plan down into multiple steps.  We'll go over the entire plan initially to make sure it's feasible and correct, but the implementation will be done one step at a time.  These are my initial ideas for the overall plan.  Review these steps and suggest any changes/improvements/potential pitfalls that you can think of.

### Step 1: Monomer Database Table Model

This will be the model for the pop up's table of monomers.  In the near future, we may switch away from Qt, so we want to keep the business logic independent of Qt's table model.

- To accomplish this, we'll create a MonomerDatabaseFilter object that stores the current filtering settings from the filtering pop up as well as the current monomer sorting (the field and sort direction).  This object will have a method to return the number of filters applied (used to populate the "# filters applied" label) and a method to return a text description of the filtering (used to populate the description next to the filtering button).
- For filtering based on the polymer type, note that we may need to allow all monomer types, or we may want to display two monomer types at a time (either peptides and chem or nucleic acid and chem).  Based on that, we may want to create a bit flag with bits for peptide, nucleic acid, and chem.  (Remember than ChainType::DNA is unused - DNA molecules are categorized as ChainType::RNA.)
- We will also have a function that takes a MonomerDatabase instance and a MonomerDatabaseFilter instance and returns a vector of MonomerInfo instances containing a sorted list of all monomers that match the filtering criteria.
- For performance reasons, I suspect we should have a separate function that applies filtering based on values typed into the line edit above the table.  That function would take a search string and a vector of MonomerInfos and return a filtered vector.  That way, we don't need to repeat the database query every time the user types a character into the line edit.  The filtering should search both the monomer symbol and the monomer name
- We'll also create a subclass of QAbstractItemModel with a setMonomers method that takes a vector of MonomerInfo and populates the table to match the spec.


### Step 2: The button and pop up

We want to implement an initial version of the side bar button and monomer database pop up.  In order to keep things simpler, this step will leave out the atomistic image of the monomer as well as the text description of the filtering that's right above the image.  The database pop up and filtering pop up should each have their own ui files.

- for the side bar button, use `src\schrodinger\sketcher\icons\monomer_db.svg` as the icon.  I haven't added that icon to the qrc file yet, so you'll need to do that.
- there should be a small button to the left of the monomer database button.  Initially, that button should be blank.  When the SketcherWidget indicates that a DrawMonomerSceneTool is active and using a monomer from the monomer database, that button should become pressed and the button's label should update to the monomer's symbol.  If the symbol is too large, shorten it the same way we shorten the monomer graphics items (i.e. fade out).
- for now, put the monomer database buttons immediately below the "unclassified" buttons (note that we'll be removing the "unclassified" buttons in the next step)
- for the filtering button, use the icons from `F:\repos\src\mmshare\python\modules\schrodinger\ui\qt\standard\icons\icon_data\filter-menu`.  Copy them into this repo.
- For the "Polymer type" combo box, use different texts in the combo box than what's shown in the spec.  Use "Peptide", "Nucleic Acid" in place of RNA, and "Structure" in place of CHEM.
- In the filtering pop up, the "Monomer type" combo box is only relevant when polymer type is set to peptide.  Disable it otherwise.  Additionally, the "Natural analog" combo box is only relevant when polymer type is set to peptide or nucleic acid.  Disable is otherwise.  "Natural analog" combo box's contents should be different between peptide and nucleic acid, as shown on the spec.
- In the "Natural analog:" combo box, "Any or None" indicates that no filtering should be done based on natural analog.   "None" indicates the natural analog must be an empty string or the unknown residue, i.e. "X" for peptides and "N" for nucleic acids.  Current databases will use "X" or "N" to indicate "None", but we'll be switching to empty string in SHARED-12734, so we should count them both.  "Any" indicates that natural analog can be any value other than None (i.e. empty or "X"/"N").  The other strings indicate that the natural analog field should match that string.  Leave "dR" out of the nucleic acid combo box.  Any sugar with a natural analog of deoxyribose will use "R", not "dR", in the natural analog field.
- Make sure that the table updates its contents if a new monomer database is loaded.
- In the table, at most one row can be selected.  Selection should always correspond to the Scene's active scene tool (as determined by examining the SketcherWidget settings).  If the active scene tool isn't present in the table, no rows should be selected.  Selecting a row in the table should immediately update the scene tool, but not close the pop up.  (Note that the spec says that "first item should probably be selected by default", but these instructions should override that.)

### Step 3: Remove the "Unclassified" buttons and simplify the monomer button pop ups

- Remove the "Unclasified" buttons from the side bar
- The pop ups that appear when a user long-clicks on a monomer button should be simplified to only show the monomers from the MonomerDatabase's m_core_monomers_db instead of all of the custom monomers
- The monomer button pops us should also have an additional button showing the same folder icon as the monomer database button (i.e. monomer_db.svg), but with a letter(s) in the upper-right corner that matches the current monomer.  Clicking on this button causes the monomer database pop up to pop up from the monomer database side bar button (i.e. not from the current monomer button pop up).  The monomer database pop up should have its filtering set to the relevant polymer type and natural analog, with all other filtering cleared.
- With this change, we can remove the code that updates the monomer button pop ups when the database updates.

### Step 4: The atomistic monomer image

Add the atomistic image of the monomer contents to the pop up, as well as the textual description of the filtering that's above the image.  The image can be generated using the functions from image_generation.h
