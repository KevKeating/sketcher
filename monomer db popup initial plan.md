We need to implement new side bar buttons that pop up a widget that allows the user to search the monomer database.  Selecting a monomer in this widget will update the SketcherModel in order switch the scene tool to a DrawMonomerSceneTool using the selected monomer.

## General information
- The Jira ID for this case is SKETCH-2827.  I've downloaded the image attachments for that case as well as the Google Doc of the spec and placed them in `F:\Documents\sketcher\SKETCH-2827`.  In `Dual Mode Sketcher v1 - design updates.docx`, we're implementing the interface described in the "Longer term approach:" subsection of the "Using the Monomer DB" section.
- Note that there has been some additional offline discussion regarding the spec.  If any of my instructions deviate from the spec, it's probably as a result of this offline discussion, but you should double check with me to be sure.
- There are two components of the spec that were not implementing yet: the "Matching SMILES" filter and the "Recently used items" component.
- There will be two Monomer Database buttons on the side bar: one in amino_page of MonomerToolWidget and one in nucleic_page.  The amino_page button pop up allows the user to select the peptide or chem polymer types, while the nucleic_page button pop up allows the user to select RNA or chem polymer types.
- These new buttons are going to replace the "Unclassified" buttons added in 5a9c9405400cd0374511bfdcf1a0e85ff24a1917 and also dramatically simplify the monomer button pop ups.  I've given additional information about this below.

## The overall plan

This change is quite large, so I've broken the plan down into multiple steps.  We'll go over the entire plan initially to make sure it's feasible and correct, but the implementation will be done one step at a time.  These are my initial ideas for the overall plan.  Review these steps and suggest any changes/improvements/potential pitfalls that you can think of.

### Step 1: Monomer Database Table Model

This will be the model for the pop up's table of monomers.  In the near future, we may switch away from Qt, so we want to keep the business logic independent of Qt's table model.

- To accomplish this, we'll create a MonomerDatabaseFilter object that stores the current filtering settings from the filtering pop up as well as the current monomer sorting (the field and sort direction).  This object will have a method to return the number of filters applied (used to populate the "# filters applied" label) and a method to return a text description of the filtering (used to populate the description next to the filtering button).
- We will also have a function that takes a MonomerDatabase instance and a MonomerDatabaseFilter instance and returns a vector of MonomerInfo instances containing a sorted list of all monomers that match the filtering criteria.
- For performance reasons, I suspect we should have a separate function that applies filtering based on values typed into the line edit above the table.  That function would take a search string and a vector of MonomerInfos and return a filtered vector.  That way, we don't need to repeat the database query every time the user types a character into the line edit.  The filtering should search both the monomer symbol and the monomer name
- We'll also create a subclass of QAbstractItemModel with a setMonomers method that takes a vector of MonomerInfo and populates the table to match the spec.


### Step 2: Initial version of the button and pop up

We want to implement an version of the side bar button and pop up that offers the functionality that's required or straight-forward to implement, but leaves out anything that's complicated.  This initial version would include:

- the table
- the search line edit
- the filtering button and filtering pop up, excluding the Clear All link (and the "Matching SMILES" part)
- the ability to click on a monomer in the table to activate the scene tool
- the number of matches below the table
- the "No monomer database found" label, which would be implemented using a QStackedWidget

This initial version would leave out:
- the image of the monomer
- the text description of the filtering
- the footer of the pop up that shows the Update Database... and Close links
- 

The database pop up and filtering pop up should each have their own ui files.

### Step 3: Remove the "Unclassified" buttons and simplify the monomer button pop ups

- Remove the "Unclasified" buttons from the side bar
- The pop ups that appear when a user long-clicks on a monomer button should be simplified to only show the monomers from the MonomerDatabase's m_core_monomers_db, plus an additional button that shows the monomer database pop up with filtering set to the relevant polymer type and natural analog.
- With this change, we can remove the code that updates the monomer button pop ups when the database updates.

### Step 4: The link UI elements

### Step 4: The side bar showing the monomer image

Add the atomistic image of the monomer contents to the pop up, as well as the textual description of the filtering




The code in <commit id> was intended as a stop gap measure until this now pop up was implemented.  After this change we can remove...
Only reuse code from that commit if it makes sense for this new implementation.  Otherwise remove it and write something new that better fits our current needs.



Loading a new monomer database shouldn't be a common scenario.
In practice, loading a new monomer database while the pop up is open should never happen.
we want a simple solution here


- need to ability to open up the pop up with a polymer type and natural analog already set
- disable nucleic acid when in peptide mode and disable peptide when in nucleic acid mode?
- we're not implementing recently used list yet
- don't select the first item by default, that contradicts closing pane with no item selected leaves the previously active tool active
- we are implementing having the selected item's symbol appear to the left of the folder icon - use the same text as what would appear in a monomer?

- implement as much of the filtering independent of Qt as possible

Additional questions for Laura:
- Should we still have two versions of the dialog, one for peptide/chem and one for nucleic acid/chem?  Or should the dialog always allow all polymer types regardless of the current side bar tab?
- Do we want a title bar on the pop up?  That seems a bit unexpected for something that's a pop up.  I'd expect that more if it were a dialog.
