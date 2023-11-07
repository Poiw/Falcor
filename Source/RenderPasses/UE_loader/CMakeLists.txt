add_renderpass(UE_loader)

target_sources(UE_loader PRIVATE
    UE_loader.cpp
    UE_loader.h
)

target_source_group(UE_loader "RenderPasses")
