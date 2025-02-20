# 2024

| Date   | Activities                                                          |
|--------|---------------------------------------------------------------------|
| 10/16  | - Initial commits.<br>- Adapt tutorial code<br>- Work through issues with building. |
| 10/17  | - First working version of sample hold node<br>- Tested in UE 5.4 (Windows). |
| 10/18  | - Rework code and documentation.<br>- Add second node to plugin (sample hold bank)<br>- Test in branch. |
| 10/19  | - Merge sample hold bank to main.<br>- Start building shift register node/refactoring sample hold bank (on hold)<br>- Continue gathering documentation<br>- Add dust node. |
| 10/20  | - Refine documentation and folder structure<br>- Add Slew node. | 
| 10/21  | - Start on node with trigger output. |
| 10/22  | - Refine documentation structure, start wiki. <br>- Investigate existing nodes and practices. | 
| 11/20  | - Work on doucmentation, wishlist. | 
| 11/21  | - Test Mac build process and nodes (UE 5.5). |
| 11/22  | - Update documentation<br>- Test alternating polarity on Dust node, merge with main.<br>- Add width node. |
| 11/23  | - Add balance node<br>- Match existing conventions. |
| 11/24  | - Merge width and balance with main. |
| 11/25  | - Create first draft of a Wave Reader node (sample-by-sample from an audio input). |
| 11/26  | - Add stereo crossfade node. |
| 11/27  | - Convert stereo crossfade to equal power, merge with main.<br>- Remove balance node (already available in spatialisation->stereo pan). Code: [e48d011](https://github.com/matthewscharles/metasound-branches/commit/e48d011755f95b5e6f1e72648e1d3b9dfbc4392d) : [MetasoundBalanceNode.cpp](https://github.com/matthewscharles/metasound-branches/blob/e48d011755f95b5e6f1e72648e1d3b9dfbc4392d/Source/MetasoundBranches/Private/MetasoundBalanceNode.cpp).<br>- Add bool To audio node.<br>- Create first draft of EDO node.|
| 11/28  | - Create first draft of tuning node.  Note: I think these two latest nodes will run on the audio thread unless we include a trigger control.  |
| 11/29  | - Test EDO, switch input from float to integer, merge into main.  |
| 11/30  | - Create click node, test, merge into main.  |
| 12/01  | - Merge tuning into main.<br>- Update documentation, shift order of readme.  |
| 12/02  | - Test build process and all nodes on UE5.5 Windows and Mac.<br>- Clean up comments and update documentation.  |
| 12/03  | - Add clock divider node and Metasound patch example.<br>- Finish phase disperser and merge into main.  |
| 12/04  | - Attempt to update code to new API (replace old branch, not much luck yet).<br>-Add categories to readme.<br>-Remove Sample and Hold bank from collection.  |
| 12/05  | - Modify Slew and Bool To Audio to use time values.<br>- Add float version of slew node.<br>- Add SVGs for documentation.  |
| 12/06  | - Add stereo inverter node.<br>- Rename width to stereo width.  |
| 12/07  | - Add stereo gain and restore stereo balance (the metasounds stereo pan node is mono input only).<br>- Test on Windows/Mac, upload next release for testing ([0.1.1](https://github.com/matthewscharles/metasound-branches/releases/tag/v0.1.1-alpha)).<br>- Move toward separate dev branch now that there are releases available to download. |
| 12/08  | - Add passthrough trigger to click node.<br>- Add first draft of stereo cross router (in branch).<br>- Complete edge node demo, merge with dev to include in next version. <br>- Tidy up documentation and labels according to existing conventions. |
| 12/09  | - Create JSON file for nodes. |
| 12/13  | - Generate HTML files for nodes. |
| 12/14  | - Attempt zero crossing node. |
| 12/15  | - Merge zero crossing into dev.<br>- Refine docs. |
| 12/16  | - Clean up build script.<br>- Rename Click to Impulse.<br>- Improve consistency of node names and labels. |
| 12/18  | - Merge dev into main, prepare for next release. |

# 2025

| Date   | Activities                                                          |
|--------|---------------------------------------------------------------------|
| 01/03  | - Fix API warnings ([information](https://github.com/matthewscharles/metasound-branches/issues/1#issuecomment-2569965131)). |
| 01/04  | - Tidy up, test new version Mac/Windows.<br>- Identify remaining warnings for API on Windows, replace DataVertexModel with DataVertex. |
| 01/08  | - Upload next release for testing ([0.1.2](https://github.com/matthewscharles/metasound-branches/releases/tag/v0.1.2-alpha)). |
| 01/10  | - Update examples. |
| 01/14  | - Complete examples, minor updates to docs. |
| 01/15  | - Add first version of [demo video](https://www.youtube.com/watch?v=HgudP5fzDWM). |
| 01/21  | - Update documentation.<br>- Add trigger output to Shift Register. |
| 02/19  | - Add Array Reverse and Array Sum nodes (working in the Sequence branch).  Troubleshoot issues with Reverse |
| 02/20 | - Start new board for sequence-related ideas.<br>- Start work on printing arrays for debugging purposes. |