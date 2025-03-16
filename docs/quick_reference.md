# Quick Reference for Plugin Development
These notes are the starting point for mapping out a plugin template.

## Overview
- Vertex names (`METASOUND_PARAM`)
- Operator class (inherit from `TExecutableOperator`)
  - **Public**
    - Constructor
    - GetNodeInfo (`FNodeClassMetadata`)
    - *DeclareVertexInterface* (`FVertexInterface`)
    - *GetInputs* (`FDataReferenceCollection`)
    - *GetOutputs* (`FDataReferenceCollection`)
    - *CreateOperator* (`TUniquePtr<IOperator>`)
    - Execute (`void`)
  - **Private**
    - Inputs
    - Outputs
    - Internal variables
- Node Class (inherit from `FNodeFacade`)

Items in *italics* must use the namespace described in the vertex names.

## Workflows
### Add an input
- [ ] Add a new `METASOUND_PARAM` to the vertex names in the node's namespace
- [ ] Add a reference to the input buffer to operator parameters (constructor)
- [ ] Initialise a corresponding variable
- [ ] Add the new input (`TInputDataVertex`) to `DeclareVertexInterface`[^1].
- [ ] Add DataReference to GetInputs
- [ ] Add GetDataReadReferenceOrConstructWithVertexDefault to CreateOperator
- [ ] Add the input as an argument to `MakeUnique<*node*Operator>`
- [ ] Add private variable for the input

### Add a trigger input
- [ ] Add a pre-trigger lambda function
- [ ] Add an on-trigger lambda function

```C++
InputTrigger->ExecuteBlock(
// Pre-trigger
  [](int32 StartFrame, int32 EndFrame)
  {
      
  },

  // On-trigger
  [&](int32 StartFrame, int32 EndFrame)
  {
      
  }
)
```

### Add a trigger output
- [ ] Advance block
```C++
OutputTrigger->AdvanceBlock();
```
### Verify namespaces
Check that the correct namespace (as defined alongside the vertex names) is used in the following locations:
- [ ] `DeclareVertexInterface`
- [ ] `GetInputs`
- [ ] `GetOutputs`
- [ ] `CreateOperator`

## Misc information
### Get sample rate
```CPP
int32 SampleRate = InParams.OperatorSettings.GetSampleRate();
```

### Get number of frames

If there is an audio pin to measure from:
```CPP
int32 NumFrames = InputSignal->Num();
```

Otherwise:
```CPP
const int32 NumFrames = InParams.OperatorSettings.GetNumFramesPerBlock();
```

or in the constructor initialisation list:
```CPP
NumFrames (InSettings.GetNumFramesPerBlock())
```

## Update nodes from tutorials for the new API

- This can generally be handled by a global find and replace:

1)
```C++
const FDataReferenceCollection& InputCollection = InParams.InputDataReferences; 
````

to
```C++
const FInputVertexInterfaceData& InputData = InParams.InputData;
```
---
2)
```C++
InputCollection.GetDataReadReferenceOrConstruct
```
to
```C++
InputCollection.GetDataReadReferenceOrConstructWithVertexDefault
```
---

3)
```C++
DataVertexModel
```
to
```
DataVertex
```
---
See the post by SynthNudibranch [here](https://forums.unrealengine.com/t/tutorial-creating-metasound-nodes-in-c-quickstart/559789).

## References

- [Creating MetaSound Nodes in C++ Quickstart](https://dev.epicgames.com/community/learning/tutorials/ry7p/unreal-engine-creating-metasound-nodes-in-c-quickstart)
- [Factory method pattern (Wikipedia)](https://en.wikipedia.org/wiki/Factory_method_pattern)
- [Epic Games Coding Standards](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine?application_version=5.4)

[^1]: This determines the order in which they appear on the onscreen node.
