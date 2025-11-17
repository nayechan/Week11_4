#pragma once

class AnimSequenceEditorState;
class UWorld;
struct ID3D11Device;

// Bootstrap helpers to construct/destroy per-tab animation editor state
class AnimSequenceEditorBootstrap
{
public:
    static AnimSequenceEditorState* CreateEditorState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice);
    static void DestroyEditorState(AnimSequenceEditorState*& State);
};
