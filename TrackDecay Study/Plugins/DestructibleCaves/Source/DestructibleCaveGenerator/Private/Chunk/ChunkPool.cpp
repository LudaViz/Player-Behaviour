// Copyright 2025 J2K2. All Rights Reserved.

#include "Chunk/ChunkPool.h"
#include "Engine/World.h"
#include "Core/ChunkManager.h"

UChunkPool::UChunkPool()
    : CurrentPoolSize(0)
    , TargetPoolSize(0)
    , MaxPoolSize(0)
    , MinPoolSize(1)
    , StepPerTick(1)
    , ResizeState(EChunkPoolResizeState::None)
{
}

void UChunkPool::Initialize(int32 InitialSize, int32 InMaxSize,int32 InMinSize, int32 InStepSize, TSubclassOf<ATerrainChunk> InChunkClass)
{
    CleanupPool();

    ChunkClass = InChunkClass;
    MaxPoolSize = InMaxSize;
    //MinPoolSize = 32; // > 3x3x3
    MinPoolSize=FMath::RoundUpToPowerOfTwo(InMinSize);
    StepPerTick = InStepSize;

    TargetPoolSize = FMath::Clamp(FMath::RoundUpToPowerOfTwo(InitialSize), MinPoolSize, MaxPoolSize);
    for (int i = 0; i < TargetPoolSize; i++)
    {
        if (ATerrainChunk* NewChunk = CreateNewChunk())
        {
            ReturnedChunks.Add(NewChunk);
            ++CurrentPoolSize;
        }
    }
    ResizeState = EChunkPoolResizeState::None;
}

void UChunkPool::RequestResize(int32 NewSize)
{
    UE_LOG(LogDCG, Warning, TEXT("PoolResized %d->%d"),CurrentPoolSize,NewSize);
    TargetPoolSize = FMath::Clamp(FMath::RoundUpToPowerOfTwo(NewSize), MinPoolSize, MaxPoolSize);
    ResizeState = (TargetPoolSize > CurrentPoolSize) ? EChunkPoolResizeState::Expanding :
                  (TargetPoolSize < CurrentPoolSize) ? EChunkPoolResizeState::Shrinking :
                  EChunkPoolResizeState::None;
}

void UChunkPool::TickResize()
{
    if (ResizeState == EChunkPoolResizeState::None)
        return;

    const int32 Diff = TargetPoolSize - CurrentPoolSize;
    const int32 Step = FMath::Min(FMath::Abs(Diff), StepPerTick);

    for (int32 i = 0; i < Step; ++i)
    {
        if (Diff > 0)
        {
            if (ATerrainChunk* NewChunk = CreateNewChunk())
            {
                ReturnedChunks.Add(NewChunk);
                ++CurrentPoolSize;
            }
        }
        else if (Diff < 0)
        {
            if (ReturnedChunks.Num() > 0)
            {
                //ATerrainChunk* ChunkToDestroy = ReturnedChunks.Pop();
                auto It = ReturnedChunks.CreateIterator(); // unordered
                ATerrainChunk* ChunkToDestroy = *It;
                ReturnedChunks.Remove(ChunkToDestroy); // 또는 Remove(Chunk)
                
                ChunkToDestroy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                ChunkToDestroy->Destroy();
                --CurrentPoolSize;
            }
        }
    }

    if (CurrentPoolSize == TargetPoolSize)
    {
        ResizeState = EChunkPoolResizeState::None;
    }

    //UE_LOG(LogDCG, Warning, TEXT("Resized to %d"), CurrentPoolSize)
}

ATerrainChunk* UChunkPool::RentChunk()
{
    //TRACE_CPUPROFILER_EVENT_SCOPE_STR("UChunkPool::RentChunk")
    if (ReturnedChunks.Num() == 0)
    {
        // Ensure we always return a chunk: double the pool size and begin expansion
        int32 NewTargetSize = FMath::Clamp(FMath::RoundUpToPowerOfTwo(CurrentPoolSize * 2), MinPoolSize, MaxPoolSize);
        if (NewTargetSize > CurrentPoolSize)
        {
            RequestResize(NewTargetSize);
        }

        // Create one chunk immediately for return
        ATerrainChunk* NewChunk = CreateNewChunk();
        if (NewChunk)
        {
            NewChunk->ResetChunk();
            ++CurrentPoolSize;
            RentedChunks.Add(NewChunk);
            NewChunk->SetChunkState(ETerrainChunkState::Idle);
        }
        else
        {
            UE_LOG(LogDCG, Error, TEXT("Failed to create chunk during RentChunk"));
        }
        return NewChunk;
    }

    //ATerrainChunk* Chunk = ReturnedChunks.Pop();
    auto It = ReturnedChunks.CreateIterator(); // unordered
    ATerrainChunk* Chunk = *It;
    ReturnedChunks.Remove(Chunk); // 또는 Remove(Chunk)
    
    Chunk->ResetChunk();
    RentedChunks.Add(Chunk);
    Chunk->SetChunkState(ETerrainChunkState::Idle);
    return Chunk;
}

void UChunkPool::ReturnChunk(ATerrainChunk* Chunk)
{
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("UChunkPool::ReturnChunk")
    if (RentedChunks.Remove(Chunk) > 0)
    {
        Chunk->ResetChunk();
        if (!ReturnedChunks.Contains(Chunk))
            ReturnedChunks.Add(Chunk);
    }
    
    // Check if we should shrink
    
    if (ReturnedChunks.Num() > CurrentPoolSize / 2)
    {
        int32 ShrinkTarget = FMath::Clamp(CurrentPoolSize/2/**2 / 3*/, MinPoolSize, MaxPoolSize);
        RequestResize(ShrinkTarget);
    }
}

void UChunkPool::CleanupPool()
{
    for (ATerrainChunk* Chunk : ReturnedChunks)
    {
        if (Chunk && !Chunk->IsPendingKillPending())
        {
            Chunk->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            Chunk->Destroy();
        }
    }
    ReturnedChunks.Empty();

    for (ATerrainChunk* Chunk : RentedChunks)
    {
        if (Chunk && !Chunk->IsPendingKillPending())
        {
            Chunk->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            Chunk->Destroy();
        }
    }
    RentedChunks.Empty();

    CurrentPoolSize = 0;
    TargetPoolSize = 0;
    ResizeState = EChunkPoolResizeState::None;
}


ATerrainChunk* UChunkPool::CreateNewChunk() const
{
    /*UWorld* World = GEngine ? GEngine->GetWorldFromContextObjectChecked(this) : nullptr;
    if (!World || !*ChunkClass)
        return nullptr;
        */
    UWorld* World = GetWorld(); // 더 안전하고 보편적
    if (!World || !*ChunkClass)
    {
        UE_LOG(LogDCG, Error, TEXT("UChunkPool::CreateNewChunk: Invalid World or ChunkClass"));
        return nullptr;
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    return World->SpawnActor<ATerrainChunk>(ChunkClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
}