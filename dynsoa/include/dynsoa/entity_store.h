// DynSoA Runtime SDK

#pragma once
#include "types.h"
#include <cstddef>

namespace dynsoa {

// Forward declaration to avoid circular include with layout.h
enum class LayoutKind : std::uint8_t;

void*  spawn(ArchetypeId arch, std::size_t count, void(*init_fn)(std::size_t, void*));
ViewId make_view(ArchetypeId arch);
size_t view_len(ViewId v);

void*  column(ViewId v, const char* path);

// Transient column-major block of selected components
struct MatrixBlock;
MatrixBlock acquire_matrix_block(ViewId v, const char** comps, int k, int block_rows, std::size_t offset_rows=0);
void        release_matrix_block(ViewId v, MatrixBlock* mb, bool write_back);

// Extra helpers to avoid exposing internal ViewRec to other translation units
std::size_t bytes_to_move(ViewId v);
LayoutKind  entity_current_layout(ViewId v);
void        transform_soa_to_aosoa(ViewId v, int tile);
void        transform_aosoa_to_soa(ViewId v);

} // namespace dynsoa
