// Copyright (c) mcp-unreal project contributors. Apache-2.0 license.
//
// BlueprintRoutes.cpp — HTTP routes for Blueprint query and modify operations.
// This is the most complex route file — it exposes Blueprint graph internals
// for AI-driven editing.
//
// See IMPLEMENTATION.md §3.4 and §5.1.

#include "MCPUnrealUtils.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"

namespace MCPUnreal {

  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  /** Load a Blueprint by path, returning nullptr with error on failure. */
  static UBlueprint* LoadBlueprintByPath(const FString& Path,
                                         const FHttpResultCallback& OnComplete) {
    if (Path.IsEmpty()) {
      SendError(OnComplete, TEXT("blueprint_path is required"));
      return nullptr;
    }

    UObject* Obj = StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Path);
    UBlueprint* Blueprint = Cast<UBlueprint>(Obj);
    if (!Blueprint) {
      SendError(OnComplete, FString::Printf(TEXT("Blueprint not found at path '%s'"), *Path));
      return nullptr;
    }
    return Blueprint;
  }

  /** Resolve a UClass from either a full class path or a short engine class name. */
  static UClass* ResolveClassByName(const FString& ClassName) {
    FString TrimmedName = ClassName;
    TrimmedName.TrimStartAndEndInline();

    if (TrimmedName.IsEmpty()) {
      return nullptr;
    }

    if (UClass* Class = FindFirstObject<UClass>(*TrimmedName, EFindFirstObjectOptions::None)) {
      return Class;
    }

    if (UClass* Class = LoadObject<UClass>(nullptr, *TrimmedName)) {
      return Class;
    }

    if (!TrimmedName.StartsWith(TEXT("/Script/"))) {
      const FString EngineClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *TrimmedName);
      if (UClass* Class = LoadObject<UClass>(nullptr, *EngineClassPath)) {
        return Class;
      }
    }

    return nullptr;
  }

  /** Resolve a UFunction for a K2 call-function node. */
  static UFunction* ResolveFunctionByName(const FString& FunctionName,
                                          const FString& FunctionOwner,
                                          UBlueprint* Blueprint) {
    const FName FunctionFName(*FunctionName);

    if (UClass* OwnerClass = ResolveClassByName(FunctionOwner)) {
      if (UFunction* Function = OwnerClass->FindFunctionByName(FunctionFName)) {
        return Function;
      }
    }

    if (Blueprint && Blueprint->GeneratedClass) {
      if (UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(FunctionFName)) {
        return Function;
      }
    }

    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
      if (UFunction* Function = ClassIt->FindFunctionByName(FunctionFName)) {
        return Function;
      }
    }

    return nullptr;
  }

  /** UE 5.7 requires PC_Real pins to specify float or double as a subcategory. */
  static void NormalizeRealPinTypes(UEdGraphNode* Node) {
    if (!Node) {
      return;
    }

    for (UEdGraphPin* Pin : Node->Pins) {
      if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real &&
          Pin->PinType.PinSubCategory.IsNone()) {
        Pin->PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
      }
    }
  }

  static FEdGraphPinType MakePinTypeFromString(const FString& VarType) {
    FEdGraphPinType PinType;

    if (VarType.Equals(TEXT("float"), ESearchCase::IgnoreCase)) {
      PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
      PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    } else if (VarType.Equals(TEXT("double"), ESearchCase::IgnoreCase) ||
               VarType.Equals(TEXT("real"), ESearchCase::IgnoreCase)) {
      PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
      PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    } else {
      PinType.PinCategory = FName(*VarType);
    }

    return PinType;
  }

  /** Serialize a pin to JSON. */
  static TSharedPtr<FJsonObject> PinToJson(const UEdGraphPin* Pin) {
    TSharedPtr<FJsonObject> PinJson = MakeShareable(new FJsonObject());
    PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
    PinJson->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
    PinJson->SetStringField(TEXT("direction"),
                            Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    PinJson->SetStringField(TEXT("default_value"), Pin->DefaultValue);

    // Connected pin references.
    TArray<TSharedPtr<FJsonValue>> Links;
    for (const UEdGraphPin* Linked : Pin->LinkedTo) {
      TSharedPtr<FJsonObject> LinkJson = MakeShareable(new FJsonObject());
      LinkJson->SetStringField(TEXT("node_id"), Linked->GetOwningNode()->NodeGuid.ToString());
      LinkJson->SetStringField(TEXT("pin_name"), Linked->PinName.ToString());
      Links.Add(MakeShareable(new FJsonValueObject(LinkJson)));
    }
    PinJson->SetArrayField(TEXT("links"), Links);

    return PinJson;
  }

  /** Serialize a graph node to JSON. */
  static TSharedPtr<FJsonObject> NodeToJson(const UEdGraphNode* Node) {
    TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject());
    NodeJson->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
    NodeJson->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    NodeJson->SetStringField(TEXT("title"),
                             Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    NodeJson->SetNumberField(TEXT("pos_x"), Node->NodePosX);
    NodeJson->SetNumberField(TEXT("pos_y"), Node->NodePosY);
    NodeJson->SetStringField(TEXT("comment"), Node->NodeComment);

    TArray<TSharedPtr<FJsonValue>> PinsArray;
    for (const UEdGraphPin* Pin : Node->Pins) {
      PinsArray.Add(MakeShareable(new FJsonValueObject(PinToJson(Pin))));
    }
    NodeJson->SetArrayField(TEXT("pins"), PinsArray);

    return NodeJson;
  }

  // ---------------------------------------------------------------------------
  // POST /api/blueprints/list
  // ---------------------------------------------------------------------------

  static bool HandleBlueprintsList(const FHttpServerRequest& Request,
                                   const FHttpResultCallback& OnComplete) {
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> BlueprintAssets;
    AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets,
                                   true);

    TArray<TSharedPtr<FJsonValue>> ResultArray;
    for (const FAssetData& Asset : BlueprintAssets) {
      TSharedPtr<FJsonObject> BPJson = MakeShareable(new FJsonObject());
      BPJson->SetStringField(TEXT("name"), Asset.AssetName.ToString());
      BPJson->SetStringField(TEXT("path"), Asset.GetObjectPathString());

      FString ParentClass;
      Asset.GetTagValue(FBlueprintTags::ParentClassPath, ParentClass);
      BPJson->SetStringField(TEXT("parent_class"), ParentClass);

      ResultArray.Add(MakeShareable(new FJsonValueObject(BPJson)));
    }

    SendJsonArray(OnComplete, ResultArray);
    return true;
  }

  // ---------------------------------------------------------------------------
  // POST /api/blueprints/inspect
  // ---------------------------------------------------------------------------

  static bool HandleBlueprintsInspect(const FHttpServerRequest& Request,
                                      const FHttpResultCallback& OnComplete) {
    TSharedPtr<FJsonObject> Body;
    if (!ParseJsonBody(Request, Body)) {
      SendError(OnComplete, TEXT("Invalid JSON in request body"));
      return true;
    }

    UBlueprint* Blueprint =
        LoadBlueprintByPath(Body->GetStringField(TEXT("blueprint_path")), OnComplete);
    if (!Blueprint) return true;

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("name"), Blueprint->GetName());
    ResponseJson->SetStringField(TEXT("path"), Blueprint->GetPathName());
    ResponseJson->SetStringField(TEXT("parent_class"), Blueprint->ParentClass
                                                           ? Blueprint->ParentClass->GetName()
                                                           : TEXT("None"));

    // Variables.
    TArray<TSharedPtr<FJsonValue>> VarsArray;
    for (const FBPVariableDescription& Var : Blueprint->NewVariables) {
      TSharedPtr<FJsonObject> VarJson = MakeShareable(new FJsonObject());
      VarJson->SetStringField(TEXT("name"), Var.VarName.ToString());
      VarJson->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
      VarJson->SetBoolField(TEXT("is_instance_editable"),
                            Var.PropertyFlags & CPF_Edit ? true : false);
      VarsArray.Add(MakeShareable(new FJsonValueObject(VarJson)));
    }
    ResponseJson->SetArrayField(TEXT("variables"), VarsArray);

    // Function graphs.
    TArray<TSharedPtr<FJsonValue>> FuncsArray;
    for (const UEdGraph* Graph : Blueprint->FunctionGraphs) {
      TSharedPtr<FJsonObject> FuncJson = MakeShareable(new FJsonObject());
      FuncJson->SetStringField(TEXT("name"), Graph->GetName());
      FuncJson->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
      FuncsArray.Add(MakeShareable(new FJsonValueObject(FuncJson)));
    }
    ResponseJson->SetArrayField(TEXT("functions"), FuncsArray);

    // Event graphs (UbergraphPages).
    TArray<TSharedPtr<FJsonValue>> EventGraphsArray;
    for (const UEdGraph* Graph : Blueprint->UbergraphPages) {
      TSharedPtr<FJsonObject> GraphJson = MakeShareable(new FJsonObject());
      GraphJson->SetStringField(TEXT("name"), Graph->GetName());
      GraphJson->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
      EventGraphsArray.Add(MakeShareable(new FJsonValueObject(GraphJson)));
    }
    ResponseJson->SetArrayField(TEXT("event_graphs"), EventGraphsArray);

    SendJson(OnComplete, ResponseJson);
    return true;
  }

  // ---------------------------------------------------------------------------
  // POST /api/blueprints/get_graph
  // ---------------------------------------------------------------------------

  static bool HandleBlueprintsGetGraph(const FHttpServerRequest& Request,
                                       const FHttpResultCallback& OnComplete) {
    TSharedPtr<FJsonObject> Body;
    if (!ParseJsonBody(Request, Body)) {
      SendError(OnComplete, TEXT("Invalid JSON in request body"));
      return true;
    }

    UBlueprint* Blueprint =
        LoadBlueprintByPath(Body->GetStringField(TEXT("blueprint_path")), OnComplete);
    if (!Blueprint) return true;

    const FString GraphName = Body->GetStringField(TEXT("graph_name"));

    // Find the graph by name — search function graphs and event graphs.
    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs) {
      if (Graph->GetName() == GraphName) {
        TargetGraph = Graph;
        break;
      }
    }
    if (!TargetGraph) {
      for (UEdGraph* Graph : Blueprint->UbergraphPages) {
        if (Graph->GetName() == GraphName) {
          TargetGraph = Graph;
          break;
        }
      }
    }
    if (!TargetGraph) {
      SendError(OnComplete, FString::Printf(TEXT("Graph '%s' not found in Blueprint"), *GraphName));
      return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("graph_name"), TargetGraph->GetName());

    TArray<TSharedPtr<FJsonValue>> NodesArray;
    for (const UEdGraphNode* Node : TargetGraph->Nodes) {
      NodesArray.Add(MakeShareable(new FJsonValueObject(NodeToJson(Node))));
    }
    ResponseJson->SetArrayField(TEXT("nodes"), NodesArray);

    SendJson(OnComplete, ResponseJson);
    return true;
  }

  // ---------------------------------------------------------------------------
  // POST /api/blueprints/modify
  // ---------------------------------------------------------------------------

  static bool HandleBlueprintsModify(const FHttpServerRequest& Request,
                                     const FHttpResultCallback& OnComplete) {
    TSharedPtr<FJsonObject> Body;
    if (!ParseJsonBody(Request, Body)) {
      SendError(OnComplete, TEXT("Invalid JSON in request body"));
      return true;
    }

    const FString Operation = Body->GetStringField(TEXT("operation"));
    if (Operation.IsEmpty()) {
      SendError(OnComplete, TEXT("operation field is required"));
      return true;
    }

    // --- create ---
    if (Operation == TEXT("create")) {
      const FString BPName = Body->GetStringField(TEXT("name"));
      const FString PackagePath = Body->GetStringField(TEXT("package_path"));
      const FString ParentClassStr = Body->GetStringField(TEXT("parent_class"));

      if (BPName.IsEmpty()) {
        SendError(OnComplete, TEXT("name is required for create operation"));
        return true;
      }

      FString FinalPath = PackagePath.IsEmpty() ? TEXT("/Game/") : PackagePath;
      if (!FinalPath.EndsWith(TEXT("/"))) {
        FinalPath += TEXT("/");
      }

      UClass* ParentClass = AActor::StaticClass();  // Default parent.
      if (!ParentClassStr.IsEmpty()) {
        UClass* Found = FindFirstObject<UClass>(*ParentClassStr, EFindFirstObjectOptions::None);
        if (Found) {
          ParentClass = Found;
        }
      }

      UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
          ParentClass, CreatePackage(*(FinalPath + BPName)), FName(*BPName), BPTYPE_Normal,
          UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

      if (!NewBP) {
        SendError(OnComplete, TEXT("Failed to create Blueprint"), 500);
        return true;
      }

      TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
      ResponseJson->SetBoolField(TEXT("success"), true);
      ResponseJson->SetStringField(TEXT("path"), NewBP->GetPathName());
      SendJson(OnComplete, ResponseJson);
      return true;
    }

    // All other operations need an existing Blueprint.
    UBlueprint* Blueprint =
        LoadBlueprintByPath(Body->GetStringField(TEXT("blueprint_path")), OnComplete);
    if (!Blueprint) return true;

    bool bNeedsCompile = false;

    // --- add_variable ---
    if (Operation == TEXT("add_variable")) {
      const FString VarName = Body->GetStringField(TEXT("variable_name"));
      const FString VarType = Body->GetStringField(TEXT("variable_type"));
      if (VarName.IsEmpty() || VarType.IsEmpty()) {
        SendError(OnComplete, TEXT("variable_name and variable_type are required"));
        return true;
      }

      const FEdGraphPinType PinType = MakePinTypeFromString(VarType);
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
      bNeedsCompile = true;
    }
    // --- remove_variable ---
    else if (Operation == TEXT("remove_variable")) {
      const FString VarName = Body->GetStringField(TEXT("variable_name"));
      if (VarName.IsEmpty()) {
        SendError(OnComplete, TEXT("variable_name is required"));
        return true;
      }
      FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VarName));
      bNeedsCompile = true;
    }
    // --- add_function ---
    else if (Operation == TEXT("add_function")) {
      const FString FuncName = Body->GetStringField(TEXT("function_name"));
      if (FuncName.IsEmpty()) {
        SendError(OnComplete, TEXT("function_name is required"));
        return true;
      }

      UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
          Blueprint, FName(*FuncName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
      FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, false,
                                              static_cast<UFunction*>(nullptr));
      bNeedsCompile = true;
    }
    // --- remove_function ---
    else if (Operation == TEXT("remove_function")) {
      const FString FuncName = Body->GetStringField(TEXT("function_name"));
      if (FuncName.IsEmpty()) {
        SendError(OnComplete, TEXT("function_name is required"));
        return true;
      }

      for (UEdGraph* Graph : Blueprint->FunctionGraphs) {
        if (Graph->GetName() == FuncName) {
          FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
          break;
        }
      }
      bNeedsCompile = true;
    }
    // --- add_node ---
    else if (Operation == TEXT("add_node")) {
      const FString GraphName = Body->GetStringField(TEXT("graph_name"));
      const FString NodeClass = Body->GetStringField(TEXT("node_class"));
      const double PosX = Body->GetNumberField(TEXT("pos_x"));
      const double PosY = Body->GetNumberField(TEXT("pos_y"));

      if (GraphName.IsEmpty() || NodeClass.IsEmpty()) {
        SendError(OnComplete, TEXT("graph_name and node_class are required"));
        return true;
      }

      // Find the graph.
      UEdGraph* Graph = nullptr;
      for (UEdGraph* G : Blueprint->FunctionGraphs) {
        if (G->GetName() == GraphName) {
          Graph = G;
          break;
        }
      }
      if (!Graph) {
        for (UEdGraph* G : Blueprint->UbergraphPages) {
          if (G->GetName() == GraphName) {
            Graph = G;
            break;
          }
        }
      }
      if (!Graph) {
        SendError(OnComplete, FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
        return true;
      }

      // Find the node class.
      UClass* NodeUClass = FindFirstObject<UClass>(*NodeClass, EFindFirstObjectOptions::None);
      if (!NodeUClass || !NodeUClass->IsChildOf(UEdGraphNode::StaticClass())) {
        SendError(OnComplete, FString::Printf(TEXT("Node class '%s' not found"), *NodeClass));
        return true;
      }

      UEdGraphNode* NewNode = NewObject<UEdGraphNode>(Graph, NodeUClass);
      NewNode->CreateNewGuid();

      if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(NewNode)) {
        FString FunctionName;
        if (Body->TryGetStringField(TEXT("function_name"), FunctionName) &&
            !FunctionName.IsEmpty()) {
          FString FunctionOwner;
          Body->TryGetStringField(TEXT("function_owner"), FunctionOwner);

          if (const UFunction* Function =
                  ResolveFunctionByName(FunctionName, FunctionOwner, Blueprint)) {
            CallFunctionNode->SetFromFunction(Function);
          } else {
            SendError(OnComplete,
                      FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
            return true;
          }
        }
      } else if (UK2Node_DynamicCast* DynamicCastNode = Cast<UK2Node_DynamicCast>(NewNode)) {
        FString TargetClassName;
        Body->TryGetStringField(TEXT("target_class"), TargetClassName);
        if (TargetClassName.IsEmpty()) {
          Body->TryGetStringField(TEXT("class_name"), TargetClassName);
        }

        if (!TargetClassName.IsEmpty()) {
          UClass* TargetClass = ResolveClassByName(TargetClassName);
          if (!TargetClass) {
            SendError(OnComplete,
                      FString::Printf(TEXT("Target class '%s' not found"), *TargetClassName));
            return true;
          }
          DynamicCastNode->TargetType = TargetClass;
        }
      }

      FString VariableName;
      if (Body->TryGetStringField(TEXT("variable_name"), VariableName) &&
          !VariableName.IsEmpty()) {
        if (UK2Node_VariableGet* VariableGetNode = Cast<UK2Node_VariableGet>(NewNode)) {
          VariableGetNode->VariableReference.SetSelfMember(FName(*VariableName));
        } else if (UK2Node_VariableSet* VariableSetNode = Cast<UK2Node_VariableSet>(NewNode)) {
          VariableSetNode->VariableReference.SetSelfMember(FName(*VariableName));
        }
      }

      NewNode->PostPlacedNewNode();
      NewNode->AllocateDefaultPins();
      NormalizeRealPinTypes(NewNode);
      NewNode->NodePosX = static_cast<int32>(PosX);
      NewNode->NodePosY = static_cast<int32>(PosY);
      Graph->AddNode(NewNode, false, false);
      NewNode->ReconstructNode();
      NormalizeRealPinTypes(NewNode);

      TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
      ResponseJson->SetBoolField(TEXT("success"), true);
      ResponseJson->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
      SendJson(OnComplete, ResponseJson);
      return true;
    }
    // --- delete_node ---
    else if (Operation == TEXT("delete_node")) {
      const FString GraphName = Body->GetStringField(TEXT("graph_name"));
      const FString NodeId = Body->GetStringField(TEXT("node_id"));

      if (GraphName.IsEmpty() || NodeId.IsEmpty()) {
        SendError(OnComplete, TEXT("graph_name and node_id are required"));
        return true;
      }

      UEdGraph* Graph = nullptr;
      for (UEdGraph* G : Blueprint->FunctionGraphs) {
        if (G->GetName() == GraphName) {
          Graph = G;
          break;
        }
      }
      if (!Graph) {
        for (UEdGraph* G : Blueprint->UbergraphPages) {
          if (G->GetName() == GraphName) {
            Graph = G;
            break;
          }
        }
      }
      if (!Graph) {
        SendError(OnComplete, FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
        return true;
      }

      FGuid TargetGuid;
      FGuid::Parse(NodeId, TargetGuid);
      for (UEdGraphNode* Node : Graph->Nodes) {
        if (Node->NodeGuid == TargetGuid) {
          Graph->RemoveNode(Node);
          bNeedsCompile = true;
          break;
        }
      }
    }
    // --- connect_pins ---
    else if (Operation == TEXT("connect_pins")) {
      const FString GraphName = Body->GetStringField(TEXT("graph_name"));
      const FString SourceNodeId = Body->GetStringField(TEXT("source_node_id"));
      const FString SourcePinName = Body->GetStringField(TEXT("source_pin"));
      const FString TargetNodeId = Body->GetStringField(TEXT("target_node_id"));
      const FString TargetPinName = Body->GetStringField(TEXT("target_pin"));

      if (GraphName.IsEmpty() || SourceNodeId.IsEmpty() || SourcePinName.IsEmpty() ||
          TargetNodeId.IsEmpty() || TargetPinName.IsEmpty()) {
        SendError(
            OnComplete,
            TEXT(
                "graph_name, source_node_id, source_pin, target_node_id, target_pin are required"));
        return true;
      }

      UEdGraph* Graph = nullptr;
      for (UEdGraph* G : Blueprint->FunctionGraphs) {
        if (G->GetName() == GraphName) {
          Graph = G;
          break;
        }
      }
      if (!Graph) {
        for (UEdGraph* G : Blueprint->UbergraphPages) {
          if (G->GetName() == GraphName) {
            Graph = G;
            break;
          }
        }
      }
      if (!Graph) {
        SendError(OnComplete, FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
        return true;
      }

      FGuid SrcGuid, TgtGuid;
      FGuid::Parse(SourceNodeId, SrcGuid);
      FGuid::Parse(TargetNodeId, TgtGuid);

      UEdGraphPin* SourcePin = nullptr;
      UEdGraphPin* TargetPin = nullptr;

      for (UEdGraphNode* Node : Graph->Nodes) {
        if (Node->NodeGuid == SrcGuid) {
          SourcePin = Node->FindPin(FName(*SourcePinName));
        }
        if (Node->NodeGuid == TgtGuid) {
          TargetPin = Node->FindPin(FName(*TargetPinName));
        }
      }

      if (!SourcePin || !TargetPin) {
        SendError(OnComplete, TEXT("Source or target pin not found"));
        return true;
      }

      if (SourcePin->LinkedTo.Contains(TargetPin)) {
        SourcePin->BreakLinkTo(TargetPin);
      }

      const UEdGraphSchema* Schema = Graph->GetSchema();
      const bool bConnected = Schema && Schema->TryCreateConnection(SourcePin, TargetPin);
      if (!bConnected) {
        SourcePin->MakeLinkTo(TargetPin);
      }
      bNeedsCompile = true;
    }
    // --- disconnect_pins ---
    else if (Operation == TEXT("disconnect_pins")) {
      const FString GraphName = Body->GetStringField(TEXT("graph_name"));
      const FString NodeId = Body->GetStringField(TEXT("node_id"));
      const FString PinName = Body->GetStringField(TEXT("pin_name"));

      if (GraphName.IsEmpty() || NodeId.IsEmpty() || PinName.IsEmpty()) {
        SendError(OnComplete, TEXT("graph_name, node_id, pin_name are required"));
        return true;
      }

      UEdGraph* Graph = nullptr;
      for (UEdGraph* G : Blueprint->FunctionGraphs) {
        if (G->GetName() == GraphName) {
          Graph = G;
          break;
        }
      }
      if (!Graph) {
        for (UEdGraph* G : Blueprint->UbergraphPages) {
          if (G->GetName() == GraphName) {
            Graph = G;
            break;
          }
        }
      }

      if (Graph) {
        FGuid NodeGuid;
        FGuid::Parse(NodeId, NodeGuid);
        for (UEdGraphNode* Node : Graph->Nodes) {
          if (Node->NodeGuid == NodeGuid) {
            if (UEdGraphPin* Pin = Node->FindPin(FName(*PinName))) {
              Pin->BreakAllPinLinks();
              bNeedsCompile = true;
            }
            break;
          }
        }
      }
    }
    // --- set_pin_value ---
    else if (Operation == TEXT("set_pin_value")) {
      const FString GraphName = Body->GetStringField(TEXT("graph_name"));
      const FString NodeId = Body->GetStringField(TEXT("node_id"));
      const FString PinName = Body->GetStringField(TEXT("pin_name"));
      const FString PinValue = Body->GetStringField(TEXT("value"));

      if (GraphName.IsEmpty() || NodeId.IsEmpty() || PinName.IsEmpty()) {
        SendError(OnComplete, TEXT("graph_name, node_id, pin_name are required"));
        return true;
      }

      UEdGraph* Graph = nullptr;
      for (UEdGraph* G : Blueprint->FunctionGraphs) {
        if (G->GetName() == GraphName) {
          Graph = G;
          break;
        }
      }
      if (!Graph) {
        for (UEdGraph* G : Blueprint->UbergraphPages) {
          if (G->GetName() == GraphName) {
            Graph = G;
            break;
          }
        }
      }

      if (Graph) {
        FGuid NodeGuid;
        FGuid::Parse(NodeId, NodeGuid);
        for (UEdGraphNode* Node : Graph->Nodes) {
          if (Node->NodeGuid == NodeGuid) {
            if (UEdGraphPin* Pin = Node->FindPin(FName(*PinName))) {
              if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
                  Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject ||
                  Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class ||
                  Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass ||
                  Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface) {
                UObject* DefaultObject = nullptr;
                if (!PinValue.IsEmpty() && PinValue != TEXT("None")) {
                  DefaultObject = LoadObject<UObject>(nullptr, *PinValue);
                }
                Pin->DefaultObject = DefaultObject;
                Pin->DefaultValue.Reset();
              } else {
                Pin->DefaultValue = PinValue;
              }
              bNeedsCompile = true;
            }
            break;
          }
        }
      }
    }
    // --- compile ---
    else if (Operation == TEXT("compile")) {
      bNeedsCompile = true;
    } else {
      SendError(OnComplete, FString::Printf(TEXT("Unknown operation: '%s'"), *Operation));
      return true;
    }

    // Auto-compile after mutation.
    bool bCompiled = false;
    if (bNeedsCompile) {
      FKismetEditorUtilities::CompileBlueprint(Blueprint);
      bCompiled = true;
      UE_LOG(LogMCPUnreal, Log, TEXT("Compiled Blueprint '%s' after '%s' operation"),
             *Blueprint->GetName(), *Operation);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetBoolField(TEXT("success"), true);
    ResponseJson->SetBoolField(TEXT("compiled"), bCompiled);
    SendJson(OnComplete, ResponseJson);
    return true;
  }

  // ---------------------------------------------------------------------------
  // Registration
  // ---------------------------------------------------------------------------

  void RegisterBlueprintRoutes(TSharedPtr<IHttpRouter> Router, TArray<FHttpRouteHandle>& Handles) {
    Handles.Add(Router->BindRoute(FHttpPath(TEXT("/api/blueprints/list")),
                                  EHttpServerRequestVerbs::VERB_POST,
                                  FHttpRequestHandler::CreateStatic(&HandleBlueprintsList)));

    Handles.Add(Router->BindRoute(FHttpPath(TEXT("/api/blueprints/inspect")),
                                  EHttpServerRequestVerbs::VERB_POST,
                                  FHttpRequestHandler::CreateStatic(&HandleBlueprintsInspect)));

    Handles.Add(Router->BindRoute(FHttpPath(TEXT("/api/blueprints/get_graph")),
                                  EHttpServerRequestVerbs::VERB_POST,
                                  FHttpRequestHandler::CreateStatic(&HandleBlueprintsGetGraph)));

    Handles.Add(Router->BindRoute(FHttpPath(TEXT("/api/blueprints/modify")),
                                  EHttpServerRequestVerbs::VERB_POST,
                                  FHttpRequestHandler::CreateStatic(&HandleBlueprintsModify)));

    UE_LOG(LogMCPUnreal, Verbose, TEXT("Registered Blueprint routes (4 endpoints)"));
  }

}  // namespace MCPUnreal
