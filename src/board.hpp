#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <functional>

namespace StumblingBotV2 {

#define BIT(b) (1ull << (uint64_t)(b))

#define INCREMENT_OP(T) \
    constexpr T &operator++(T& v) { return v = T((int)v + 1); } \
    constexpr T operator++(T& v, int) { T tmp(v); ++v; return tmp; } \
    constexpr T &operator+=(T& v1, T v2) { return v1 = T((int)v1 + (int)v2); } \
    constexpr T operator+(T v1, T v2) { T tmp(v1); tmp += v2; return tmp; } \
    constexpr T &operator+=(T& v1, int v2) { return v1 = T((int)v1 + (int)v2); } \
    constexpr T operator+(T v1, int v2) { T tmp(v1); tmp += v2; return tmp; } \
    \
    constexpr T &operator--(T& v) { return v = T((int)v - 1); } \
    constexpr T operator--(T& v, int) { T tmp(v); --v; return tmp; } \
    constexpr T &operator-=(T& v1, T v2) { return v1 = T((int)v1 - (int)v2); } \
    constexpr T operator-(T v1, T v2) { T tmp(v1); tmp -= v2; return tmp; } \
    constexpr T &operator-=(T& v1, int v2) { return v1 = T((int)v1 - (int)v2); } \
    constexpr T operator-(T v1, int v2) { T tmp(v1); tmp -= v2; return tmp; }

#define BITWISE_OP(T) \
    constexpr T &operator|=(T& v1, T v2) { return v1 = T((int)v1 | (int)v2); } \
    constexpr T operator|(T v1, T v2) { T tmp(v1); tmp |= v2; return tmp; } \
    constexpr T &operator&=(T& v1, T v2) { return v1 = T((int)v1 & (int)v2); } \
    constexpr T operator&(T v1, T v2) { T tmp(v1); tmp &= v2; return tmp; } \
    constexpr T &operator^=(T& v1, T v2) { return v1 = T((int)v1 ^ (int)v2); } \
    constexpr T operator^(T v1, T v2) { T tmp(v1); tmp ^= v2; return tmp; } \
    constexpr T operator~(T v) { return T(~int(v)); }

enum class Square : uint8_t {
    A8, B8, C8, D8, E8, F8, G8, H8,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A1, B1, C1, D1, E1, F1, G1, H1,
    NONE,

    IT_BEGIN = A8,
    IT_END = NONE,
};

INCREMENT_OP(Square)
BITWISE_OP(Square)

Square str2square(const std::string &);
std::string square2str(Square);

enum class Side : uint8_t {
    WHITE = 0,
    BLACK = 1,
};

INCREMENT_OP(Side)
BITWISE_OP(Side)

constexpr Side other_side(Side side) {
    return side ^ Side::BLACK;
}

enum class Piece : uint8_t {
    WHITE_PAWN = 0,
    WHITE_KNIGHT = 2,
    WHITE_BISHOP = 4,
    WHITE_ROOK = 6,
    WHITE_QUEEN = 8,
    WHITE_KING = 10,
    
    BLACK_PAWN = WHITE_PAWN | (int)Side::BLACK,
    BLACK_KNIGHT = WHITE_KNIGHT | (int)Side::BLACK,
    BLACK_BISHOP = WHITE_BISHOP | (int)Side::BLACK,
    BLACK_ROOK = WHITE_ROOK | (int)Side::BLACK,
    BLACK_QUEEN = WHITE_QUEEN | (int)Side::BLACK,
    BLACK_KING = WHITE_KING | (int)Side::BLACK,

    NONE,

    IT_BEGIN = WHITE_PAWN,
    IT_END = NONE,

    PAWN = WHITE_PAWN,
    KNIGHT = WHITE_KNIGHT,
    BISHOP = WHITE_BISHOP,
    ROOK = WHITE_ROOK,
    QUEEN = WHITE_QUEEN,
    KING = WHITE_KING,

    COLORLESS_IT_BEGIN = PAWN,
    COLORLESS_IT_END = NONE,
};

INCREMENT_OP(Piece)
BITWISE_OP(Piece)

constexpr Piece colorless_piece(Piece piece) {
    return piece & (Piece)0b1110;
}

constexpr void next_colorless_piece(Piece &piece) {
    switch (piece) {
        case Piece::PAWN: piece = Piece::KNIGHT; break;
        case Piece::KNIGHT: piece = Piece::BISHOP; break;
        case Piece::BISHOP: piece = Piece::ROOK; break;
        case Piece::ROOK: piece = Piece::QUEEN; break;
        case Piece::QUEEN: piece = Piece::KING; break;
        case Piece::KING: piece = Piece::NONE; break;
        default: piece = Piece::NONE;
    }
}

constexpr Piece side2piece(Side side, Piece piece) {
    return (piece & (Piece)0b1110) | (Piece)side;
}

constexpr Piece piece_other_side(Piece piece) {
    return piece ^ (Piece)Side::BLACK;
}

constexpr Side piece2side(Piece piece) {
    return (Side)piece & Side::BLACK;
}

Piece str2piece(char);
char piece2str(Piece);
Piece piece_next(Piece);

enum class Castling : uint8_t {
    NONE = 0,
    WHITE_OO  = 1,
    WHITE_OOO = 2,
    BLACK_OO  = 4,
    BLACK_OOO = 8,

    OO = WHITE_OO | BLACK_OO,
    OOO = WHITE_OOO | BLACK_OOO,
    WHITE = WHITE_OO | WHITE_OOO,
    BLACK = BLACK_OO | BLACK_OOO,
};

BITWISE_OP(Castling)

constexpr uint64_t white_oo_mask = BIT(Square::F1) | BIT(Square::G1);

constexpr uint64_t white_ooo_attack_mask = BIT(Square::C1) | BIT(Square::D1);
constexpr uint64_t white_ooo_occ_mask = BIT(Square::B1) | BIT(Square::C1) | BIT(Square::D1);

constexpr uint64_t black_oo_mask = BIT(Square::F8) | BIT(Square::G8);

constexpr uint64_t black_ooo_attack_mask = BIT(Square::C8) | BIT(Square::D8);
constexpr uint64_t black_ooo_occ_mask = BIT(Square::B8) | BIT(Square::C8) | BIT(Square::D8);

constexpr Square rank_square(Square sq) {
    return sq & (Square)0b111000u;
}
constexpr Square file_square(Square sq) {
    return sq & (Square)0b000111u;
}
constexpr uint64_t rank_mask(Square sq) {
    return 0x0000'0000'0000'00FFull << (uint8_t)rank_square(sq);
}
constexpr uint64_t file_mask(Square sq) {
    return 0x0101'0101'0101'0101ull << (uint8_t)file_square(sq);
}

enum class BoardFlags {
    NONE = 0,
    CHECK = 1,
    DOUBLE_CHECK = 2,
    MATE = 4,
    STALEMATE = 8,
    INSUFFICIENT_MATERIAL = 16,
    REPETITION = 32,
    FIFTY_MOVES = 64,
    DRAW = STALEMATE | INSUFFICIENT_MATERIAL | REPETITION | FIFTY_MOVES,
    ENDED = MATE | DRAW,
};

struct Move {
    Square from;
    Square to;

    Piece piece;
    Piece promotion_piece;
    Piece captured_piece;

    Castling castling;

    std::string to_string() const;
};

class MoveContainer {
public:
    static inline constexpr int s_max_legal_move_count = 218;

    using Array = std::array<Move, s_max_legal_move_count>;

    size_t size() const;
    const Move &at(size_t i) const;
    Array::iterator begin();
    Array::iterator end();

    Move &emplace();

private:
    std::array<Move, s_max_legal_move_count> _moves;
    size_t _count = 0;
};

BITWISE_OP(BoardFlags)

class Bitboards {
public:
    uint64_t get_occupancy() const;
    uint64_t get_per_side_occupancy(Side side) const;
    uint64_t get_bitboard(Piece piece) const;

    void clear();

    Piece piece_at(Square) const;
    void set_piece(Piece, Square);
    void reset_piece(Piece, Square);

private:
    std::array<uint64_t, (size_t)Piece::NONE> _bitboards {0};
    std::array<uint64_t, 2> _per_side_occupancy = {0};
    uint64_t _occupancy = 0;
    std::array<Piece, 64> _pieces;
};

class Zobrist {
public:
    static const Zobrist &get();

    std::array<std::array<uint64_t, 64>, (size_t)Piece::NONE> pieces;
    uint64_t side;
    std::array<uint64_t, 16> castling;
    std::array<uint64_t, 8> en_passant;

private:
    Zobrist();
};

class Board {
public:
    Board();

    bool import_fen(const std::string &fen);
    std::string export_fen() const;
    uint64_t hash() const;

    void draw() const;
    
    bool has_flag(BoardFlags) const;
    MoveContainer generate_moves() const;

    void make_move(const Move &);
    bool make_move_from_string(const std::string &);

    Side side_to_move() const;
    Bitboards get_bitboards() const;
    bool castling_allowed(Castling) const;

    static inline const std::string s_starting_position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

private:
    struct _Precomputed {
    public:
        static const _Precomputed &get();

        uint64_t between_ray(Square sq1, Square sq2) const;
        uint64_t diagonals(Square sq1) const;

        std::array<uint64_t, 64> bishop_occupancy;

        std::array<std::vector<uint64_t>, 64> rook_moves;
        std::array<std::vector<uint64_t>, 64> bishop_moves;

    private:
        _Precomputed();

        static uint64_t _rook_move_slow(Square sq, uint64_t blockers);
        static uint64_t _bishop_move_slow(Square sq, uint64_t blockers);

        std::vector<uint64_t> _generate_blockers(uint64_t occ);
        void _precompute_rook_moves();
        void _precompute_bishop_occupancy();
        void _precompute_bishop_moves();

        void _precompute_between_rays();
        void _precompute_diagonals();
        std::array<uint64_t, 64*64> _between_rays;
        std::array<uint64_t, 64> _lines;
        std::array<uint64_t, 64> _diagonals;
    };

    Square _en_passant_capture() const;

    uint64_t _pawn_moves(Side side, uint64_t bit) const;
    constexpr uint64_t _pawn_captures(Side side, Square sq) const;
    constexpr uint64_t _knight_moves(Square sq) const;
    constexpr uint64_t _king_moves(Square sq) const;
    template <bool ExcludeKing>
    uint64_t _rook_moves(Square sq) const;
    template <bool ExcludeKing>
    uint64_t _bishop_moves(Square sq) const;
    template <bool ExcludeKing>
    uint64_t _queen_moves(Square sq) const;
    template <bool ExcludeKing>
    uint64_t _piece_moves(Side side, Square sq, Piece piece) const;
    template <bool ExcludeKing>
    uint64_t _piece_captures(Side side, Square sq, Piece piece) const; // Same as _piece_moves except for pawns

    static constexpr uint64_t _rook_occupancy(Square sq);

    void _generate_pawn_moves(MoveContainer &) const;
    void _generate_king_moves(MoveContainer &) const;
    void _generate_basic_piece_moves(MoveContainer &, Piece, std::function<uint64_t(const Board*, Square)> moves_function) const;

    bool _import_fen_placement(const std::string &str);
    bool _import_fen_side_to_move(const std::string &str);
    bool _import_fen_castling(const std::string &str);
    bool _import_fen_en_passant(const std::string &str);
    bool _import_fen_clocks(const std::string &str1, const std::string &str2);

    void _set_flags();

    Bitboards _bitboards;
    
    BoardFlags _flags;
    uint64_t _check_mask;
    uint64_t _attack_mask;
    uint64_t _pinned_mask;
    std::array<uint64_t, 64> _pinned_allowed_rays;
    // Only exist for this stupid case with a pawn pinned from doing an en passant
    // 8/2p5/8/KP5r/7k/8/8/8 b moves c7c5 b5c6.
    uint64_t _en_passant_edge_case;

    Side _side_to_move;

    Castling _castling_state;
    Square _en_passant_target;

    int _half_move_clock;
    int _full_move_count;
};

constexpr uint64_t Board::_pawn_captures(Side side, Square sq) const {
    int factor = side == Side::WHITE ? -1 : 1;
    Square file = file_square(sq);
    constexpr Square first_file = file_square(Square::A1);
    constexpr Square last_file = file_square(Square::H1);
    
    uint64_t mask = file != first_file ? BIT((int)sq + 8*factor - 1) : 0;
    mask |= file != last_file ? BIT((int)sq + 8*factor + 1) : 0;

    return mask;
}

constexpr uint64_t Board::_knight_moves(Square sq) const {
    Square file = file_square(sq);
    // Every square except the 2 right most files
    constexpr uint64_t left_mask = ~(file_mask(Square::H1) | file_mask(Square::G1));
    // Every square except the 2 left most files
    constexpr uint64_t right_mask = ~(file_mask(Square::A1) | file_mask(Square::B1));
    constexpr uint64_t default_mask =
        BIT(Square::B8) | BIT(Square::D8) |
        BIT(Square::A7) | BIT(Square::E7) |
        BIT(Square::A5) | BIT(Square::E5) |
        BIT(Square::B4) | BIT(Square::D4);

    int shift = (int)sq - (uint8_t)Square::C6;
    uint64_t mask = shift < 0 ? default_mask >> (-shift) : default_mask << shift;
    
    // Remove squares that go around the board left <-> right with the bit shift.
    mask &= (uint8_t)file <= 3 ? left_mask : right_mask;

    return mask;
}

constexpr uint64_t Board::_king_moves(Square sq) const {
    Square file = file_square(sq);
    // Every square except the 2 right most files
    constexpr uint64_t left_mask = ~(file_mask(Square::H1) | file_mask(Square::G1));
    // Every square except the 2 left most files
    constexpr uint64_t right_mask = ~(file_mask(Square::A1) | file_mask(Square::B1));
    constexpr uint64_t default_mask =
        BIT(Square::A8) | BIT(Square::B8) | BIT(Square::C8) |
        BIT(Square::A7) |                   BIT(Square::C7) |
        BIT(Square::A6) | BIT(Square::B6) | BIT(Square::C6);

    int shift = (int)sq - (uint8_t)Square::B7;
    uint64_t mask = shift < 0 ? default_mask >> (-shift) : default_mask << shift;
    
    // Remove squares that go around the board left <-> right with the bit shift.
    mask &= (uint8_t)file <= 3 ? left_mask : right_mask;

    return mask;
}

// constexpr uint64_t Board::_king_moves(Square sq) const {
//     // Every square except the 2 right most files
//     constexpr uint64_t left_mask = ~(file_mask(Square::H1) | file_mask(Square::G1));
//     // Every square except the 2 left most files
//     constexpr uint64_t right_mask = ~(file_mask(Square::A1) | file_mask(Square::B1));
//     // Every square except the 2 top most ranks
//     constexpr uint64_t bottom_mask = ~(rank_mask(Square::A8) | rank_mask(Square::A7));
//     // Every square except the 2 bottom most ranks
//     constexpr uint64_t top_mask = ~(rank_mask(Square::A1) | rank_mask(Square::A2));

//     constexpr uint64_t king_bit = BIT(Square::A8);
//     constexpr uint64_t default_mask =
//         std::rotr(king_bit, 9) | std::rotr(king_bit, 8) | std::rotr(king_bit, 7) |
//         std::rotr(king_bit, 1) |                          std::rotl(king_bit, 1) |
//         std::rotr(king_bit, 7) | std::rotr(king_bit, 8) | std::rotr(king_bit, 9);

//     uint64_t mask;
//     if (std::is_constant_evaluated()) {
//         mask = std::rotl(default_mask, (int)sq);
//     } else {
//         mask = _rotl64(default_mask, (int)sq);
//     }
    
//     // Remove squares that go around the board left <-> right with the bit shift.
//     Square file = file_square(sq);
//     mask &= (uint8_t)file <= 3 ? left_mask : right_mask;
//     mask &= sq <= Square::H5 ? top_mask : bottom_mask;

//     return mask;
// }

constexpr uint64_t Board::_rook_occupancy(Square sq) {
    uint64_t rank = rank_mask(sq);
    uint64_t file = file_mask(sq);
    uint64_t mask = rank | file;
    mask &= ~BIT(sq);

    constexpr uint64_t outer_files = file_mask(Square::A1) | file_mask(Square::H1);
    constexpr uint64_t outer_ranks = rank_mask(Square::A1) | rank_mask(Square::A8);

    mask &= ~(rank & outer_files);
    mask &= ~(file & outer_ranks);

    return mask;
}

template <bool ExcludeKing>
uint64_t Board::_rook_moves(Square sq) const {
    uint64_t rook_occ = _rook_occupancy(sq);
    uint64_t occ = _bitboards.get_occupancy();
    if constexpr (ExcludeKing) {
        occ &= ~_bitboards.get_bitboard(side2piece(_side_to_move, Piece::KING));
    }

    uint64_t index = _pext_u64(occ, rook_occ);
    return _Precomputed::get().rook_moves[(int)sq][index];
}

template <bool ExcludeKing>
uint64_t Board::_bishop_moves(Square sq) const {
    const _Precomputed &precomputed = _Precomputed::get();
    uint64_t bishop_occ = precomputed.bishop_occupancy[(int)sq];
    uint64_t occ = _bitboards.get_occupancy();
    if constexpr (ExcludeKing) {
        occ &= ~_bitboards.get_bitboard(side2piece(_side_to_move, Piece::KING));
    }

    uint64_t index = _pext_u64(occ, bishop_occ);
    return precomputed.bishop_moves[(int)sq][index];
}

template <bool ExcludeKing>
uint64_t Board::_queen_moves(Square sq) const {
    return _rook_moves<ExcludeKing>(sq) | _bishop_moves<ExcludeKing>(sq);
}

template <bool ExcludeKing>
uint64_t Board::_piece_moves(Side side, Square sq, Piece piece) const {
    switch (piece) {
        case Piece::WHITE_PAWN:
        case Piece::BLACK_PAWN:
            return _pawn_moves(side, BIT(sq));
        case Piece::WHITE_KNIGHT:
        case Piece::BLACK_KNIGHT:
            return _knight_moves(sq);
        case Piece::WHITE_BISHOP:
        case Piece::BLACK_BISHOP:
            return _bishop_moves<ExcludeKing>(sq);
        case Piece::WHITE_ROOK:
        case Piece::BLACK_ROOK:
            return _rook_moves<ExcludeKing>(sq);
        case Piece::WHITE_QUEEN:
        case Piece::BLACK_QUEEN:
            return _queen_moves<ExcludeKing>(sq);
        case Piece::WHITE_KING:
        case Piece::BLACK_KING:
            return _king_moves(sq);

        default:
            return 0;
    }
}

template <bool ExcludeKing>
uint64_t Board::_piece_captures(Side side, Square sq, Piece piece) const {
    switch (piece) {
        case Piece::WHITE_PAWN:
        case Piece::BLACK_PAWN:
            return _pawn_captures(side, sq);
        case Piece::WHITE_KNIGHT:
        case Piece::BLACK_KNIGHT:
            return _knight_moves(sq);
        case Piece::WHITE_BISHOP:
        case Piece::BLACK_BISHOP:
            return _bishop_moves<ExcludeKing>(sq);
        case Piece::WHITE_ROOK:
        case Piece::BLACK_ROOK:
            return _rook_moves<ExcludeKing>(sq);
        case Piece::WHITE_QUEEN:
        case Piece::BLACK_QUEEN:
            return _queen_moves<ExcludeKing>(sq);
        case Piece::WHITE_KING:
        case Piece::BLACK_KING:
            return _king_moves(sq);

        default:
            return 0;
    }
}

}