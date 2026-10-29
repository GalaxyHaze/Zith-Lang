#pragma once

#include "cache/cache-identity.hpp"
#include "hir/hir-module.hpp"
#include "memory/dyn-array.hpp"
#include "memory/string-interner.hpp"
#include "types/type-id.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zith::cache {

using HirExprId = uint32_t;

// Compact type encoding used inside artifacts.  Primitive types are stored as
// a single tag; composite types reference other compact type ids stored in the
// same section.  This is self-contained and independent of the in-memory
// TypeIntern so a reader does not need to reconstruct the full type table to
// validate identity.
enum class CompactTypeKind : uint8_t {
    Error        = 0,
    Never        = 1,
    Void         = 2,
    Bool         = 3,
    Char         = 4,
    Int          = 5,
    Float        = 6,
    Ptr          = 7,
    Array        = 8,
    Struct       = 9,
    Fn           = 10,
    Optional     = 11,
    Failable     = 12,
    Slice        = 13,
    Enum         = 14,
    Union        = 15,
    TypeVar      = 16,
    GenericParam = 17,
    Incomplete   = 18,
    Opaque       = 19,
    OpaqueTagged = 22,
    Pack         = 20,
    Dyn          = 21,
    Trait        = 23,
};

struct CompactType {
    CompactTypeKind kind = CompactTypeKind::Error;
    uint8_t int_width    = 0;        // IntWidth / FloatWidth
    uint8_t flags        = 0;        // is_mut | ownership | is_signed
    uint32_t ref0        = 0;        // pointee / elem / base / inner / ret
    uint32_t ref1        = 0;        // count / def_id
    std::vector<uint32_t> args;      // fn params / app args / pack members
    std::vector<uint32_t> arg_names; // pack member names (string ids)
};

// Definition payload for named composite types referenced by CompactType.
// The compact type table carries only def ids, so this table lets a cache reader
// rebuild private/internal composite definitions that are not part of the
// exported DeclRecord table.
struct CompactStructDef {
    std::string name;
    uint32_t name_id = 0;
    std::vector<uint32_t> field_name_ids;
    std::vector<uint32_t> field_type_ids;
    std::vector<uint64_t> foreign_field_offsets;
    std::vector<uint32_t> type_arg_ids;
    // Validated C-record ABI. Only foreign records populated through the C
    // binder set these flags; native Zith structs keep aggregate ABI.
    bool hasForeignLayout      = false;
    bool foreignAbiIsSingleI64 = false;
    uint64_t foreignSizeBytes  = 0;
    uint64_t foreignAlignBytes = 0;
    uint32_t explicitAlignmentBytes = 0;
};

struct CompactEnumVariant {
    std::string name;
    uint32_t name_id     = 0;
    int64_t discriminant = 0;
};

struct CompactEnumDef {
    std::string name;
    uint32_t name_id       = 0;
    uint32_t underlying_id = ~uint32_t{0};
    std::vector<CompactEnumVariant> variants;
};

struct CompactUnionDef {
    std::string name;
    uint32_t name_id = 0;
    std::vector<uint32_t> member_type_ids;
    bool is_raw = false;
};

// A dependency on another module.  Resolved at load time by matching the
// dependency's canonical path and public ABI hash against what the importer
// currently sees.
struct DependencyRecord {
    std::string canonical_path;
    std::string import_key;
    uint32_t public_abi_hi = 0;
    uint32_t public_abi_lo = 0;
};

// Canonical type id assigned to a project-local runtime opaque tag.
struct CompactCanonicalMapping {
    uint64_t hi         = 0;
    uint64_t lo         = 0;
    uint32_t runtime_id = 0;
};

enum class CompactSymKind : uint8_t {
    Fn,
    Struct,
    Trait,
    Interface,
    Enum,
    Alias,
    Variable,
    Module,
    Component,
    Union,
    Asset,
    Word,
    Context,
};

enum class CompactVisibility : uint8_t { Private, Public, Module };

// One exported or module-visible declaration.  References metadata by compact
// string id and references types by compact type id.  For generic declarations
// `template_index` points into the templates section; otherwise it is invalid.
struct DeclRecord {
    std::string name;
    CompactSymKind kind = CompactSymKind::Variable;
    CompactVisibility visibility = CompactVisibility::Private;
    int32_t mod_depth = 0;
    uint32_t name_id = 0;
    uint32_t type_id = 0; // primary type (fn sig / struct type / alias target)
    uint32_t template_index = ~uint32_t{0};
    uint32_t body_fn_index = ~uint32_t{0}; // index into code section, if concrete
    std::vector<uint32_t> field_name_ids;                // struct/enum/component fields
    std::vector<uint32_t> field_type_ids;
    std::vector<uint32_t> method_decl_indices; // methods, as decl indices
    bool is_extern = false;
    bool discardable = false;
};

// Generic parameter with optional bounds (compact type ids).
struct GenericParamRecord {
    std::string name;
    uint32_t name_id = 0;
    std::vector<uint32_t> bound_type_ids;
};

// Blueprint for a generic struct/function/component/etc.  The body of a generic
// function is NOT lowered to HIR in sec5; instead the blueprint holds the
// declarative shape needed to re-instantiate it.
struct TemplateBlueprint {
    std::string name;
    uint32_t name_id    = 0;
    CompactSymKind kind = CompactSymKind::Fn;
    uint32_t declaration_index = ~uint32_t{0};
    uint32_t function_index = ~uint32_t{0};
    std::vector<GenericParamRecord> params;
    std::vector<GenericParamRecord> method_params; // for implement blocks
    uint32_t return_type_id = ~uint32_t{0};        // fn return type, if present
    std::vector<uint32_t> param_type_ids;          // fn param types
    std::vector<uint32_t> param_name_ids;          // fn param names
    std::vector<uint32_t> field_name_ids;          // struct/component fields
    std::vector<uint32_t> field_type_ids;
    // Canonicalized field layout order (indices into field_* arrays).  Empty
    // when the declaration has no fields or the canonical order equals the
    // declared order.
    std::vector<uint32_t> canonical_field_order;
    std::vector<uint32_t> method_decl_indices; // methods, as decl indices
    std::vector<uint32_t> trait_type_ids;      // implemented traits / extends
    bool is_extern = false;
    bool is_variadic = false;
    bool is_state = false;
    bool is_foreign_c = false;
    bool uses_tailcc = false;
};

// HIR expression in compact form.  Mirrors hir::HirExpr but uses compact ids
// and std::vector so it is self-contained for serialization.
enum class CompactExprKind : uint8_t {
    Literal,
    Binary,
    Unary,
    Let,
    Var,
    Call,
    Ret,
    Branch,
    Jump,
    Phi,
    Assign,
    Index,
    Field,
    StructLiteral,
    ArrayLiteral,
    EnumValue,
    SlotAlloca,
    SlotStore,
    SlotLoad,
    SlotAddr,
    MakeNone,
    MakeSome,
    MakeSlice,
    Cast,
    LayoutIntrinsic,
    StateTailCall,
    UnionCheck,
    Cleanup,
    GlobalConstLoad,
    MakeDyn,
    DynCall,
    MakeOpaque,
    OpaqueCast,
    OpaqueCheck,
    RuntimePanic,
    CanonicalType,
    Pipe,
    PipeDo,
    PipeCurrent,
    ExternalGlobalAddr,
};

enum class CompactBinaryOp : uint8_t {
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
    Xor,
    Shl,
    Shr,
};
enum class CompactUnaryOp : uint8_t { Neg, Not, BitNot, Ref, Deref };

enum class CompactSafetyMode : uint8_t { RequiredCheck, Guarded, Intrinsic, Raw };
enum class CompactSafetyProofKind : uint8_t {
    BoundsValid,
    OptionalPresent,
    UnionMemberMatches,
    OpaqueTypeMatches,
};

struct CompactExpr {
    CompactExprKind kind = CompactExprKind::Literal;
    uint32_t type_id     = 0;
    uint32_t ref_a       = 0; // lhs / operand / object / callee / target
    uint32_t ref_b       = 0; // rhs / index / value
    uint32_t ref_c       = 0; // then_block / field index / version
    uint32_t ref_d       = 0; // else_block
    uint32_t ref_e       = 0; // Cast::from / LayoutIntrinsic::which / operand type / call fn_type
    uint32_t ref_f       = 0; // LayoutIntrinsic::field_index
    // Canonical string-table identity for a direct HIR function reference.
    // The in-memory HirFunctionId is never written to the artifact.
    uint32_t resolved_function_name_id = ~uint32_t{0};
    // Compact signature identity disambiguates overloads with the same name.
    uint32_t resolved_function_signature_type_id = ~uint32_t{0};
    uint32_t variadic_slice_param = ~uint32_t{0};
    bool is_variadic_slice = false;
    bool auto_collect_tail = false;
    uint32_t alignmentBytes = 0;
    uint8_t op           = 0; // binary/unary op
    uint8_t flags        = 0; // is_array / literal sub-tag
    CompactSafetyMode safety = CompactSafetyMode::RequiredCheck;
    int64_t int_val      = 0;
    double flt_val       = 0.0;
    uint32_t name_id     = 0;        // let/var name
    std::vector<uint32_t> args;      // call args / phi incoming / literal values
    std::vector<uint32_t> arg_types; // call argument types
    std::vector<uint64_t> ints;      // wide value payloads (canonical type ids, i128)
};

struct CompactBasicBlock {
    std::vector<uint32_t> insts;
    uint32_t terminator = ~uint32_t{0};
};

struct CompactFunction {
    std::string name;
    uint32_t name_id = 0;
    uint32_t signature_type_id = ~uint32_t{0};
    std::vector<uint32_t> param_type_ids;
    std::vector<uint32_t> param_name_ids;
    std::vector<uint32_t> param_slot_ids;
    uint32_t return_type_id = 0;
    std::vector<CompactBasicBlock> blocks;
    std::vector<CompactExpr> exprs;
    uint32_t instance_index         = ~uint32_t{0};
    bool is_extern                  = false;
    bool is_foreign_c               = false;
    bool is_variadic                = false;
    bool is_state                   = false;
    bool uses_tailcc                = false;
    uint32_t machine_id             = 0;
    uint32_t machine_return_type_id = 0;
    uint32_t variadic_slice_param   = ~uint32_t{0};
    uint32_t foreign_adapter_name_id = ~uint32_t{0};
    bool discardable                = false;
};

struct CompactGlobalConst {
    uint32_t name_id        = 0;
    uint32_t type_id        = 0;
    uint32_t init_expr      = ~uint32_t{0};
    uint32_t alignmentBytes = 0;
};

struct CompactVTable {
    uint32_t name_id = 0;
    std::vector<uint32_t> slot_function_name_ids;
    std::vector<uint32_t> slot_function_signature_type_ids;
};

struct HirSlotAttrsRecord {
    uint32_t slot     = 0;
    uint8_t ownership = 0;
    uint8_t consumed  = 0;
    bool nonNull      = false;
    bool volatileSlot = false;
};

struct HirCallAttrsRecord {
    uint32_t expr_id = 0;
    std::vector<uint32_t> arg_escapes;
    uint32_t returns_arg = ~uint32_t{0};
};

struct HirFnAttrsRecord {
    uint32_t fn_index       = 0;
    uint8_t return_consumed = 0;
    bool nonNull            = false;
    bool noAlias            = false;
    bool readOnly           = false;
    bool noCapture          = false;
    bool discardable        = false;
};

struct HirSafetyProofRecord {
    uint32_t expr_id = 0;
    CompactSafetyProofKind kind = CompactSafetyProofKind::BoundsValid;
    uint32_t operand_a = ~uint32_t{0};
    uint32_t operand_b = ~uint32_t{0};
    uint32_t operand_c = ~uint32_t{0};
};

// Monomorphized instances produced by the comptime pass. The HIR bodies already
// carry the concrete names and call targets, so this record is a stable summary
// for cache inspection and future re-instantiation without rerunning sema.
struct InstantiationRecord {
    std::string canonical_module;
    std::string mangled;
    std::string template_name;
    std::vector<std::string> arg_types;
};

// One assert/check site inside a `#[test]` function. The runner resolves a
// failure event (test name plus 1-based failure id) to its source location.
struct TestFailureLocation {
    std::string test_name;
    uint32_t failure_id = 0;
    std::string path;
    uint32_t line = 0;
    uint32_t column = 0;
};

// The full decoded artifact.  Built by the reader and consumed by the hydrator.
struct Artifact {
    std::string canonical_path;
    std::string module_name;
    FrontendIdentity frontend_identity;
    SemanticCompatibilityProfile semantic_profile;
    CanonicalModuleIdentity module_identity;
    uint32_t hir_schema_version = kHirSchemaVersion;
    ContentFingerprint source_fingerprint;
    uint32_t public_abi_hi  = 0;
    uint32_t public_abi_lo  = 0;
    uint64_t required_features = kRequiredHirFeatures;
    uint64_t optional_features = 0;
    std::vector<std::string> strings;         // sec2 string table
    std::vector<std::string> paths;           // sec2 path table
    std::vector<CompactType> types;           // sec2 type table
    std::vector<DependencyRecord> deps;       // sec1 dependency list
    std::vector<DeclRecord> decls;            // sec3
    std::vector<TemplateBlueprint> templates; // sec4
    std::vector<CompactFunction> functions;   // sec5
    std::vector<CompactExpr> exprs;           // sec5 module-level expression pool
    std::vector<CompactGlobalConst> globals;  // sec5
    std::vector<CompactVTable> vtables;       // sec5
    // Canonical identities that have received a project-local opaque tag.
    std::vector<CompactCanonicalMapping> canonical_mappings;
    // Definition tables used to restore composite types regardless of whether
    // they appear among exported DeclRecord entries.
    std::vector<CompactStructDef> struct_defs;
    std::vector<CompactEnumDef> enum_defs;
    std::vector<CompactUnionDef> union_defs;
    // sec6 ownership/call/fn residual facts (HirAttrs)
    std::vector<HirSlotAttrsRecord> attrs_slots;
    std::vector<HirCallAttrsRecord> attrs_calls;
    std::vector<HirFnAttrsRecord> attrs_fns;
    std::vector<HirSafetyProofRecord> attrs_safety_proofs;
    std::vector<InstantiationRecord> instantiations;
    // sec8 debug: assert/check locations for the native test runner.
    std::vector<TestFailureLocation> test_failure_locations;
    struct OptionalSection {
        uint16_t id = 0;
        uint16_t schema_version = 1;
        std::vector<uint8_t> payload;
    };
    std::vector<OptionalSection> optional_sections;
};

} // namespace zith::cache
