// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WeaponVisualComponent.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;

/**
 * UWeaponVisualComponent — 角色背上武器显示组件
 *
 * 读取 UInventoryComponent 的已装备武器 ID，从 DT_WeaponConfig 加载 MeshAsset，
 * 把 WeaponMesh 挂到角色骨骼背部 socket；所有 Role（以及后续敌人）复用。
 * 战斗拔出/收回后续通过 SetWeaponVisible() 控制，当前阶段始终显示背上状态。
 */
UCLASS(ClassGroup = (Inventory), Blueprintable, meta = (BlueprintSpawnableComponent))
class HOLE_API UWeaponVisualComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWeaponVisualComponent();

	virtual void BeginPlay() override;

	/** 显示/隐藏背上武器（后续战斗拔出/收回用） */
	UFUNCTION(BlueprintCallable, Category = "WeaponVisual")
	void SetWeaponVisible(bool bShow);

	/** 按当前已装备武器刷新网格（装备变化或外部调用） */
	UFUNCTION(BlueprintCallable, Category = "WeaponVisual")
	void RefreshWeaponVisual();

	/** 背部挂载 socket 名（不存在时回退到网格根节点 + BackAttachOffset） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponVisual|Config")
	FName BackSocketName = TEXT("weapon_back");

	/** socket 不存在时的回退相对偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponVisual|Config")
	FTransform BackAttachOffset = FTransform::Identity;

	/** 手部挂载 socket 名（入场拔刀通知触发时使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponVisual|Config")
	FName HandSocketName = TEXT("weapon_hand_r");

	/** 武器 Static Mesh 组件（BP 中可微调相对 Transform） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WeaponVisual")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** 由 Actor 构造器传入已创建好的武器网格组件（Actor 级默认子对象，避免嵌套子对象实例化问题） */
	void SetWeaponMeshComponent(UStaticMeshComponent* InMesh);

	/** 把武器挂到指定 socket（背部/手部通用；socket 缺失回退网格根节点 + BackAttachOffset） */
	UFUNCTION(BlueprintCallable, Category = "WeaponVisual")
	void AttachWeaponToSocket(FName SocketName);

	/** 武器是否已拔到手上（挂到 HandSocketName 时为 true；挂回背部自动复位） */
	UFUNCTION(BlueprintPure, Category = "WeaponVisual")
	bool IsWeaponDrawn() const { return bWeaponDrawn; }

private:
	/** Inventory 武器变化回调 */
	UFUNCTION()
	void HandleWeaponChanged(FName WeaponID);

	/** 缓存的主人网格体 */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CachedMesh;

	/** 显示开关（后续战斗系统控制拔出/收回） */
	bool bWeaponVisible = true;

	/** 是否已拔到手上（由 AttachWeaponToSocket 维护） */
	bool bWeaponDrawn = false;
};
