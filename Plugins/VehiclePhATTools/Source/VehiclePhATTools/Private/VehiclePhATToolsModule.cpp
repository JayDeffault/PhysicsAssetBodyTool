#include "VehiclePhATToolsModule.h"
#include "VehiclePhATToolsLog.h"
#include "VehiclePhATBodyUtils.h"
#include "VehiclePhATClipboard.h"
#include "VehiclePhATMirrorUtils.h"
#include "VehiclePhATConstraintUtils.h"
#include "VehiclePhATConvexUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "VehiclePhATTools"
static const FName VehiclePhATToolsTabName(TEXT("VehiclePhATTools"));

class SVehiclePhATToolsPanel : public SCompoundWidget
{
public:
SLATE_BEGIN_ARGS(SVehiclePhATToolsPanel){} SLATE_END_ARGS()
void Construct(const FArguments&){ Refresh(); ChildSlot [ SNew(SScrollBox)+SScrollBox::Slot()[ SNew(SVerticalBox)
+SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text(this,&SVehiclePhATToolsPanel::AssetText)]
+SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SButton).Text(LOCTEXT("Refresh","Refresh Selected PhysicsAsset")).OnClicked(this,&SVehiclePhATToolsPanel::OnRefresh)]
+SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text(this,&SVehiclePhATToolsPanel::BodiesText)]
+SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SEditableTextBox).HintText(LOCTEXT("BoneHint","Bone name for copy/paste/constraint")).Text(this,&SVehiclePhATToolsPanel::BoneText).OnTextCommitted(this,&SVehiclePhATToolsPanel::OnBoneCommitted)]
+SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SEditableTextBox).HintText(LOCTEXT("ParentHint","Parent bone for constraint")).Text(this,&SVehiclePhATToolsPanel::ParentText).OnTextCommitted(this,&SVehiclePhATToolsPanel::OnParentCommitted)]
+SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SUniformGridPanel).SlotPadding(2)
+SUniformGridPanel::Slot(0,0)[SNew(SButton).Text(LOCTEXT("CopyBody","Copy Body Settings")).OnClicked(this,&SVehiclePhATToolsPanel::CopyBody)]
+SUniformGridPanel::Slot(1,0)[SNew(SButton).Text(LOCTEXT("PasteBody","Paste Body Settings")).OnClicked(this,&SVehiclePhATToolsPanel::PasteBody)]
+SUniformGridPanel::Slot(0,1)[SNew(SButton).Text(LOCTEXT("CopyTransform","Copy Transform")).OnClicked(this,&SVehiclePhATToolsPanel::CopyTransform)]
+SUniformGridPanel::Slot(1,1)[SNew(SButton).Text(LOCTEXT("PasteTransform","Paste Transform")).OnClicked(this,&SVehiclePhATToolsPanel::PasteTransform)]
+SUniformGridPanel::Slot(0,2)[SNew(SButton).Text(LOCTEXT("Mirror","Mirror Selected Bodies")).OnClicked(this,&SVehiclePhATToolsPanel::Mirror)]
+SUniformGridPanel::Slot(1,2)[SNew(SButton).Text(LOCTEXT("Constraint","Create Constraint")).OnClicked(this,&SVehiclePhATToolsPanel::Constraint)]
+SUniformGridPanel::Slot(0,3)[SNew(SButton).Text(LOCTEXT("ConvexCreate","Create Convex Bodies")).OnClicked(this,&SVehiclePhATToolsPanel::ConvexCreate)]
+SUniformGridPanel::Slot(1,3)[SNew(SButton).Text(LOCTEXT("ConvexEdit","Edit Convex Bodies")).OnClicked(this,&SVehiclePhATToolsPanel::ConvexEdit)]
+SUniformGridPanel::Slot(0,4)[SNew(SButton).Text(LOCTEXT("Validate","Validate Physics Asset")).OnClicked(this,&SVehiclePhATToolsPanel::Validate)]]
+SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text(this,&SVehiclePhATToolsPanel::StatusText)] ]]; }
private:
UPhysicsAsset* Asset=nullptr; FName Bone; FName ParentBone; FString Status;
void Refresh(){Asset=FVehiclePhATBodyUtils::GetSelectedPhysicsAsset(); if(Asset&&Asset->SkeletalBodySetups.Num()){Bone=Asset->SkeletalBodySetups[0]->BoneName; ParentBone=Bone;} Status=Asset?TEXT("Ready."):TEXT("Select a PhysicsAsset in Content Browser.");}
FText AssetText()const{return FText::FromString(Asset?FString::Printf(TEXT("PhysicsAsset: %s"),*Asset->GetName()):TEXT("PhysicsAsset: <none>"));}
FText BodiesText()const{return FText::FromString(Asset?FString::Printf(TEXT("Bodies: %d  Constraints: %d"),Asset->SkeletalBodySetups.Num(),Asset->ConstraintSetup.Num()):TEXT("Bodies: 0"));}
FText BoneText()const{return FText::FromName(Bone);} FText ParentText()const{return FText::FromName(ParentBone);} FText StatusText()const{return FText::FromString(Status);}
void OnBoneCommitted(const FText& T,ETextCommit::Type){Bone=FName(*T.ToString());} void OnParentCommitted(const FText& T,ETextCommit::Type){ParentBone=FName(*T.ToString());}
FReply OnRefresh(){Refresh(); return FReply::Handled();}
USkeletalBodySetup* CurrentBody()const{return FVehiclePhATBodyUtils::FindBodySetup(Asset,Bone);}
FReply CopyBody(){ if(FVehiclePhATClipboard::CopyBodySettings(CurrentBody()))Status=TEXT("Body settings copied."); else Status=TEXT("Select a valid body."); return FReply::Handled();}
FReply PasteBody(){ if(!Asset)return FReply::Handled(); FScopedTransaction Tx(LOCTEXT("PasteBodyTx","Paste Vehicle Body Settings")); Asset->Modify(); if(USkeletalBodySetup* B=CurrentBody()){B->Modify(); FString M; FVehiclePhATClipboard::PasteBodySettings(B,M); FVehiclePhATBodyUtils::MarkAssetChanged(Asset); Status=M;} return FReply::Handled();}
FReply CopyTransform(){Status=FVehiclePhATClipboard::CopyTransform(CurrentBody())?TEXT("Transform copied."):TEXT("Select a valid body."); return FReply::Handled();}
FReply PasteTransform(){ if(!Asset)return FReply::Handled(); FScopedTransaction Tx(LOCTEXT("PasteTransformTx","Paste Vehicle Body Transform")); Asset->Modify(); if(USkeletalBodySetup* B=CurrentBody()){B->Modify(); FString M; FVehiclePhATClipboard::PasteTransform(B,true,true,false,true,M); FVehiclePhATBodyUtils::MarkAssetChanged(Asset); Status=M;} return FReply::Handled();}
FReply Mirror(){ if(!Asset)return FReply::Handled(); FVehiclePhATMirrorOptions O; auto Pairs=FVehiclePhATMirrorUtils::BuildMirrorPairs(Asset,O); FString M; FVehiclePhATMirrorUtils::ApplyMirror(Asset,O,Pairs,M); Status=FString::Printf(TEXT("Previewed %d pairs. %s"),Pairs.Num(),*M); return FReply::Handled();}
FReply Constraint(){ if(!Asset)return FReply::Handled(); FVehiclePhATConstraintOptions O; O.ParentBone=ParentBone; O.ChildBone=Bone; O.bUpdateExisting=true; FString M; FVehiclePhATConstraintUtils::CreateOrUpdateConstraint(Asset,O,M); Status=M; return FReply::Handled();}
FReply ConvexCreate(){Status=TEXT("MVP convex tool: use FVehiclePhATConvexUtils::AddConvexFromPoints from future vertex picker; safe backend is implemented."); return FReply::Handled();}
FReply ConvexEdit(){Status=TEXT("MVP convex edit: point-cloud replace backend is implemented; interactive vertex editor is reserved for next pass."); return FReply::Handled();}
FReply Validate(){auto R=FVehiclePhATBodyUtils::ValidatePhysicsAsset(Asset); Status=FString::Printf(TEXT("Validation messages: %d"),R.Num()); for(auto& M:R){Status+=TEXT("\n- ")+M.Message;} return FReply::Handled();}
};

void FVehiclePhATToolsModule::StartupModule(){FGlobalTabmanager::Get()->RegisterNomadTabSpawner(VehiclePhATToolsTabName,FOnSpawnTab::CreateRaw(this,&FVehiclePhATToolsModule::SpawnVehiclePhATToolsTab)).SetDisplayName(LOCTEXT("TabTitle","Vehicle PhAT Tools")).SetMenuType(ETabSpawnerMenuType::Hidden); UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FVehiclePhATToolsModule::RegisterMenus)); UE_LOG(LogVehiclePhATTools,Log,TEXT("VehiclePhATTools started"));}
void FVehiclePhATToolsModule::ShutdownModule(){UToolMenus::UnRegisterStartupCallback(this); UToolMenus::UnregisterOwner(this); FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VehiclePhATToolsTabName);}
void FVehiclePhATToolsModule::RegisterMenus(){FToolMenuOwnerScoped Owner(this); UToolMenu* Menu=UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"); FToolMenuSection& S=Menu->FindOrAddSection("VehiclePhATTools"); S.AddMenuEntry("OpenVehiclePhATTools",LOCTEXT("Open","Vehicle PhAT Tools"),LOCTEXT("OpenTooltip","Open Vehicle PhAT Tools panel"),FSlateIcon(),FUIAction(FExecuteAction::CreateRaw(this,&FVehiclePhATToolsModule::OpenVehiclePhATToolsTab)));}
void FVehiclePhATToolsModule::OpenVehiclePhATToolsTab(){FGlobalTabmanager::Get()->TryInvokeTab(VehiclePhATToolsTabName);}
TSharedRef<SDockTab> FVehiclePhATToolsModule::SpawnVehiclePhATToolsTab(const FSpawnTabArgs&){return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SVehiclePhATToolsPanel)];}
IMPLEMENT_MODULE(FVehiclePhATToolsModule, VehiclePhATTools)
#undef LOCTEXT_NAMESPACE
