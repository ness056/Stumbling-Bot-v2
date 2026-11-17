#include "board.hpp"

#include <print>
#include <cassert>
#include <functional>
#include <random>

#include "utils.hpp"

namespace StumblingBotV2 {

std::string Move::to_string() const {
    std::string out = std::format("{}{}", square2str(from), square2str(to));
    if (promotion_piece != Piece::NONE) {
        out += (char)std::tolower(piece2str(promotion_piece));
    }
    return out;
}

size_t MoveContainer::size() const {
    return _count;
}

const Move &MoveContainer::at(size_t i) const {
    if (i >= _count) {
        throw std::out_of_range(std::format("Index {} was out of range.", i));
    }

    return _moves.at(i);
}

MoveContainer::Array::const_iterator MoveContainer::begin() {
    return _moves.cbegin();
}

MoveContainer::Array::const_iterator MoveContainer::end() {
    return _moves.cbegin() + _count;
}

Move &MoveContainer::emplace() {
    return _moves[_count++];
}

uint64_t Board::_Precomputed::_rook_move_slow(Square sq, uint64_t blockers) {
    uint64_t mask = 0;
    int x = (int)sq % 8;
    int y = (int)sq / 8;

    for (int x1 = x + 1; x1 < 8; x1++) {
        uint64_t bit = BIT(x1 + y*8);
        mask |= bit;
        if (bit & blockers) {
            break;
        }
    }

    for (int x1 = x - 1; x1 >= 0; x1--) {
        uint64_t bit = BIT(x1 + y*8);
        mask |= bit;
        if (bit & blockers) {
            break;
        }
    }

    for (int y1 = y + 1; y1 < 8; y1++) {
        uint64_t bit = BIT(x + y1*8);
        mask |= bit;
        if (bit & blockers) {
            break;
        }
    }

    for (int y1 = y - 1; y1 >= 0; y1--) {
        uint64_t bit = BIT(x + y1*8);
        mask |= bit;
        if (bit & blockers) {
            break;
        }
    }

    return mask;
}

uint64_t Board::_Precomputed::_bishop_move_slow(Square sq, uint64_t blockers) {
    uint64_t mask = 0;
    int x = (int)sq % 8;
    int y = (int)sq / 8;

    auto diagonal = [&](std::function<std::pair<int, int>(int)> pos) {
        for (int d = 1; d < 8; d++) {
            auto p = pos(d);
            int x1 = p.first, y1 = p.second;
            if (x1 < 0 || x1 >= 8 || y1 < 0 || y1 >= 8) {
                break;
            }

            uint64_t bit = BIT(x1 + y1*8);
            mask |= bit;
            if (bit & blockers) {
                break;
            }
        }
    };

    diagonal([&](int d) { return std::pair{ x + d, y + d }; });
    diagonal([&](int d) { return std::pair{ x - d, y - d }; });
    diagonal([&](int d) { return std::pair{ x + d, y - d }; });
    diagonal([&](int d) { return std::pair{ x - d, y + d }; });

    return mask;
}

void Board::_Precomputed::_precompute_bishop_occupancy() {
    for (Square sq = Square::IT_BEGIN; sq != Square::IT_END; sq++) {
        int sq_x = (int)sq % 8;
        int sq_y = (int)sq / 8;
        int d1 = sq_y - sq_x;
        int d2 = sq_y + sq_x;

        uint64_t mask = 0;
        for (int x = 1; x < 7; x++) {
            if (x == sq_x) {
                continue;
            }

            int y = d1 + x;
            if (0 < y && y < 7) {
                int other_sq = x + y * 8;
                mask |= BIT(other_sq);
            }
        }
            
        for (int x = 1; x < 7; x++) {
            if (x == sq_x) {
                continue;
            }

            int y = d2 - x;
            if (0 < y && y < 7) {
                int other_sq = x + y * 8;
                mask |= BIT(other_sq);
            }
        }

        bishop_occupancy[(int)sq] = mask;
    }
}

std::vector<uint64_t> Board::_Precomputed::_generate_blockers(uint64_t occ) {
    std::vector<int> bit_indices;
    for (int i = 0; i < 64; i++) {
        if (occ & BIT(i)) {
            bit_indices.push_back(i);
        }
    }

    size_t bit_count = bit_indices.size();
    size_t size = 1ull << bit_count;

    std::vector<uint64_t> blockers(size);
    for (int i = 0; i < size; i++) {
        uint64_t b = 0;
        for (int j = 0; j < bit_count; j++) {
            if (i & BIT(j)) {
                b |= BIT(bit_indices[j]);
            }
        }
        blockers[i] = b;
    }

    return blockers;
}

void Board::_Precomputed::_precompute_bishop_moves() {
    for (Square sq = Square::IT_BEGIN; sq != Square::IT_END; sq++) {
        uint64_t bishop_occ = bishop_occupancy[(int)sq];

        std::vector<uint64_t> blockers = _generate_blockers(bishop_occ);
        std::vector<uint64_t> moves(blockers.size());

        for (int i = 0; i < blockers.size(); i++) {
            uint64_t b = blockers[i];
            uint64_t index = _pext_u64(b, bishop_occ);
            moves[index] = _bishop_move_slow(sq, b);
        }

        bishop_moves[(int)sq] = moves;
    }
}

void Board::_Precomputed::_precompute_rook_moves() {
    for (Square sq = Square::IT_BEGIN; sq != Square::IT_END; sq++) {
        uint64_t rook_occ = _rook_occupancy(sq);

        std::vector<uint64_t> blockers = _generate_blockers(rook_occ);
        std::vector<uint64_t> moves(blockers.size());

        for (int i = 0; i < blockers.size(); i++) {
            uint64_t b = blockers[i];
            uint64_t index = _pext_u64(b, rook_occ);
            moves[index] = _rook_move_slow(sq, b);
        }

        rook_moves[(int)sq] = moves;
    }
}

void Board::_Precomputed::_precompute_between_rays() {
    for (Square sq1 = Square::IT_BEGIN; sq1 != Square::IT_END; sq1++) {
        int x1 = (int)sq1 % 8, y1 = (int)sq1 / 8;

        for (Square sq2 = Square::IT_BEGIN; sq2 != Square::IT_END; sq2++) {
            int x2 = (int)sq2 % 8, y2 = (int)sq2 / 8;
            uint64_t mask = 0;

            if (sq1 == sq2) {                   // same square
                mask |= BIT(sq1);
            } else if (y1 == y2) {              // same rank
                for (int x = std::min(x1, x2); x <= std::max(x1, x2); x++) {
                    mask |= BIT(y1 * 8 + x);
                }
            } else if (x1 == x2) {              // same file
                for (int y = std::min(y1, y2); y <= std::max(y1, y2); y++) {
                    mask |= BIT(y * 8 + x1);
                }
            } else if (x1 - x2 == y1 - y2) {    // diagonal y = c + x
                for (int x = std::min(x1, x2), y = std::min(y1, y2); x <= std::max(x1, x2); x++, y++) {
                    mask |= BIT(y * 8 + x);
                }
            } else if (x1 - x2 == y2 - y1) {    // diagonal y = c - x
                for (int x = std::min(x1, x2), y = std::max(y1, y2); x <= std::max(x1, x2); x++, y--) {
                    mask |= BIT(y * 8 + x);
                }
            } else {
                mask |= BIT(sq1) | BIT(sq2);
            }

            _between_rays[(int)sq1 * 64 + (int)sq2] = mask;
        }
    }
}

void Board::_Precomputed::_precompute_diagonals() {
    for (Square sq = Square::IT_BEGIN; sq != Square::IT_END; sq++) {
        uint64_t mask = BIT(sq);
        int x = (int)sq % 8;
        int y = (int)sq / 8;

        auto diagonal = [&](std::function<std::pair<int, int>(int)> pos) {
            for (int d = 1; d < 8; d++) {
                auto p = pos(d);
                int x1 = p.first, y1 = p.second;
                if (x1 < 0 || x1 >= 8 || y1 < 0 || y1 >= 8) {
                    break;
                }

                uint64_t bit = BIT(x1 + y1*8);
                mask |= bit;
            }
        };

        diagonal([&](int d) { return std::pair{ x + d, y + d }; });
        diagonal([&](int d) { return std::pair{ x - d, y - d }; });
        diagonal([&](int d) { return std::pair{ x + d, y - d }; });
        diagonal([&](int d) { return std::pair{ x - d, y + d }; });

        _diagonals[(int)sq] = mask;
    }
}

Board::_Precomputed::_Precomputed() {
    _precompute_rook_moves();
    _precompute_bishop_occupancy();
    _precompute_bishop_moves();
    _precompute_between_rays();
    _precompute_diagonals();
}

uint64_t Board::_Precomputed::between_ray(Square sq1, Square sq2) const {
    return _between_rays[(int)sq1 * 64 + (int)sq2];
}

uint64_t Board::_Precomputed::diagonals(Square sq) const {
    return _diagonals[(int)sq];
}

Square Board::_en_passant_capture() const {
    if (_en_passant_target == Square::NONE) {
        return Square::NONE;
    }

    int factor = _side_to_move == Side::WHITE ? 1 : -1;
    return _en_passant_target + (Square)(8*factor);
}

uint64_t Board::_pawn_moves(Side side, uint64_t bit) const {
    constexpr uint64_t starting_ranks_mask = rank_mask(Square::A2) | rank_mask(Square::A7);

    uint64_t mask = side == Side::WHITE ? bit >> 8 : bit << 8;
    if ((_bitboards.get_occupancy() & mask) == 0 && (bit & starting_ranks_mask)) {
        mask |= side == Side::WHITE ? bit >> 16 : bit << 16;
    }

    return mask;
}

const Board::_Precomputed &Board::_Precomputed::get() {
    static _Precomputed instance;
    return instance;
}

Square str2square(const std::string &str) {
    if (str.size() != 2) return Square::NONE;
    if (str[0] < 'a' || str[0] > 'h' || str[1] < '1' || str[1] > '8') return Square::NONE;

    return (Square)((str[0] - 'a') + (7 - (str[1] - '1')) * 8);
}

std::string square2str(Square sq) {
    if (sq == Square::NONE) return "no";

    std::string str;
    str.resize(2);

    str[0] = (char)((int)sq % 8 + 'a');
    str[1] = (char)((8 - (int)sq / 8) + '0');

    return str;
}

Piece str2piece(char c) {
    switch (c) {
        case 'P': return Piece::WHITE_PAWN; 
        case 'B': return Piece::WHITE_BISHOP;
        case 'N': return Piece::WHITE_KNIGHT;
        case 'R': return Piece::WHITE_ROOK;
        case 'Q': return Piece::WHITE_QUEEN;
        case 'K': return Piece::WHITE_KING;
        case 'p': return Piece::BLACK_PAWN;
        case 'b': return Piece::BLACK_BISHOP;
        case 'n': return Piece::BLACK_KNIGHT;
        case 'r': return Piece::BLACK_ROOK;
        case 'q': return Piece::BLACK_QUEEN;
        case 'k': return Piece::BLACK_KING;
        
        default: return Piece::NONE;
    }
}

char piece2str(Piece p) {
    switch (p) {
        case Piece::WHITE_PAWN: return 'P'; 
        case Piece::WHITE_BISHOP: return 'B';
        case Piece::WHITE_KNIGHT: return 'N';
        case Piece::WHITE_ROOK: return 'R';
        case Piece::WHITE_QUEEN: return 'Q';
        case Piece::WHITE_KING: return 'K';
        case Piece::BLACK_PAWN: return 'p';
        case Piece::BLACK_BISHOP: return 'b';
        case Piece::BLACK_KNIGHT: return 'n';
        case Piece::BLACK_ROOK: return 'r';
        case Piece::BLACK_QUEEN: return 'q';
        case Piece::BLACK_KING: return 'k';

        default: return ' ';
    }
}

Piece piece_next(Piece p) {
    switch (p) {
        case Piece::WHITE_PAWN: return Piece::WHITE_KNIGHT;
        case Piece::WHITE_KNIGHT: return Piece::WHITE_BISHOP;
        case Piece::WHITE_BISHOP: return Piece::WHITE_ROOK;
        case Piece::WHITE_ROOK: return Piece::WHITE_QUEEN;
        case Piece::WHITE_QUEEN: return Piece::WHITE_KING;
        case Piece::WHITE_KING: return Piece::BLACK_PAWN;
        case Piece::BLACK_PAWN: return Piece::BLACK_KNIGHT;
        case Piece::BLACK_KNIGHT: return Piece::BLACK_BISHOP;
        case Piece::BLACK_BISHOP: return Piece::BLACK_ROOK;
        case Piece::BLACK_ROOK: return Piece::BLACK_QUEEN;
        case Piece::BLACK_QUEEN: return Piece::BLACK_KING;
        case Piece::BLACK_KING: return Piece::NONE;

        default: return Piece::NONE;
    }
}

void Board::draw() const {
    for (Square sq = Square::IT_BEGIN; sq != Square::IT_END; sq++) {
        if ((int)sq % 8 == 0) {
            std::println("\n   +---+---+---+---+---+---+---+---+");
            std::print(" {} | ", 8 - (char)sq / 8);
        }
        std::print("{}", piece2str(_bitboards.piece_at(sq)));
        std::print(" | ");
    }
    std::println("\n   +---+---+---+---+---+---+---+---+");
    std::println("     a   b   c   d   e   f   g   h");
}

Piece Bitboards::piece_at(Square sq) const {
    return _pieces[(int)sq];
}

void Bitboards::set_piece(Piece piece, Square sq) {
    assert(piece != Piece::NONE);

    uint64_t bit = BIT(sq);
    _per_side_occupancy[(int)piece2side(piece)] |= bit;
    _occupancy |= bit;
    _bitboards[(int)piece] |= bit;
    _pieces[(int)sq] = piece;
}

void Bitboards::reset_piece(Piece piece, Square sq) {
    assert(piece != Piece::NONE);

    uint64_t not_bit = ~BIT(sq);
    _per_side_occupancy[(int)piece2side(piece)] &= not_bit;
    _occupancy &= not_bit;
    _bitboards[(int)piece] &= not_bit;
    _pieces[(int)sq] = Piece::NONE;
}

void Bitboards::clear() {
    for (auto it = _bitboards.begin(); it != _bitboards.end(); it++) {
        *it = 0ull;
    }
    for (auto it = _pieces.begin(); it != _pieces.end(); it++) {
        *it = Piece::NONE;
    }
    _per_side_occupancy[0] = 0;
    _per_side_occupancy[1] = 0;
    _occupancy = 0;
}

uint64_t Bitboards::get_occupancy() const {
    return _occupancy;
}

uint64_t Bitboards::get_per_side_occupancy(Side side) const {
    return _per_side_occupancy[(int)side];
}

uint64_t Bitboards::get_bitboard(Piece piece) const {
    return _bitboards[(int)piece];
}

Zobrist::Zobrist() {
    std::mt19937_64 rng(0x123456789ABCDEFULL);

    for (auto &piece : pieces) {
        for (auto &square : piece) {
            square = rng();
        }
    }

    side = rng();

    for (auto &v : castling) {
        v = rng();
    }

    for (auto &v : en_passant) {
        v = rng();
    }
}

const Zobrist &Zobrist::get() {
    static Zobrist instance;
    return instance;
}

uint64_t Board::hash() const {
    const auto &instance = Zobrist::get();
    uint64_t h = 0;

    for (Piece piece = Piece::IT_BEGIN; piece != Piece::IT_END; piece++) {
        uint64_t bb = _bitboards.get_bitboard(piece);

        while (bb) {
            uint64_t square = pop_idx_LSB(bb);
            h ^= instance.pieces[(int)piece][square];
        }
    }

    if (_side_to_move == Side::BLACK) {
        h ^= instance.side;
    }

    h ^= instance.castling[(int)_castling_state];

    if (_en_passant_target != Square::NONE) {
        Square en_passant_file = file_square(_en_passant_target);
        h ^= instance.en_passant[(int)en_passant_file];
    }

    return h;
}

bool Board::has_flag(BoardFlags flag) const {
    return (_flags & flag) != BoardFlags::NONE;
}

void Board::_set_flags() {
    uint64_t king_bb = _bitboards.get_bitboard(side2piece(_side_to_move, Piece::KING));
    Square king_sq = (Square)scan_idx_LSB(king_bb);
    uint64_t king_rank = rank_mask(king_sq);
    Side enemy_side = other_side(_side_to_move);

    _flags = BoardFlags::NONE;
    _check_mask = 0;
    _attack_mask = 0;
    _pinned_mask = 0;
    _en_passant_edge_case = 0;

    // Set _check_mask, _attack_mask and check flags
    for (Piece piece = Piece::COLORLESS_IT_BEGIN; piece != Piece::COLORLESS_IT_END; next_colorless_piece(piece)) {
        Piece colored_piece = side2piece(enemy_side, piece);
        uint64_t bb = _bitboards.get_bitboard(colored_piece);

        while (bb != 0) {
            Square sq = (Square)pop_idx_LSB(bb);

            uint64_t moves = _piece_captures<true>(enemy_side, sq, colored_piece);
            _attack_mask |= moves;
            if (moves & king_bb) {
                uint64_t between_ray = _Precomputed::get().between_ray(sq, king_sq);
                _check_mask |= (moves | BIT(sq)) & between_ray;
                if (!has_flag(BoardFlags::CHECK)) {
                    _flags |= BoardFlags::CHECK;
                } else if (!has_flag(BoardFlags::DOUBLE_CHECK)) {
                    _flags |= BoardFlags::DOUBLE_CHECK;
                }
            }
        }
    }

    // Set _pinned_mask
    uint64_t enemy_queen = _bitboards.get_bitboard(side2piece(enemy_side, Piece::QUEEN));
    uint64_t enemy_rook = _bitboards.get_bitboard(side2piece(enemy_side, Piece::ROOK));
    uint64_t enemy_bishop = _bitboards.get_bitboard(side2piece(enemy_side, Piece::BISHOP));
    uint64_t potential_pinners = (enemy_queen | enemy_rook) & (king_rank | file_mask(king_sq));
    potential_pinners |= (enemy_queen | enemy_bishop) & _Precomputed::get().diagonals(king_sq);

    uint64_t ray_occ_mask = ~king_bb & _bitboards.get_occupancy();
    
    while (potential_pinners) {
        Square pinner = (Square)pop_idx_LSB(potential_pinners);
        uint64_t ray = _Precomputed::get().between_ray(king_sq, pinner);
        uint64_t ray_occupancy = ray & ~BIT(pinner) & ray_occ_mask;

        if (ray_occupancy == 0) continue;

        Square pop = (Square)pop_idx_LSB(ray_occupancy);
        if (ray_occupancy == 0) {
            // Only 1 ally piece is between the king and a rook/bishop/queen, so that piece is pinned.
            _pinned_mask |= BIT(pop);
            _pinned_allowed_rays[(int)pop] = ray;
        }

        ray_occupancy &= king_rank;
        Square pop2 = (Square)pop_idx_LSB(ray_occupancy);
        if (ray_occupancy == 0 && pop2 != (Square)64) {
            _en_passant_edge_case = ray;
        }
    }

    // Set other flags
    uint64_t king_moves = ~_check_mask & _king_moves(king_sq);

    if (king_moves == 0) {
        if (has_flag(BoardFlags::CHECK)) {
            _flags |= BoardFlags::MATE;
        }
    }

    if (_half_move_clock >= 50) {
        _flags |= BoardFlags::FIFTY_MOVES;
    }
}

void Board::_generate_pawn_moves(MoveContainer &moves) const {
    Side enemy_side = other_side(_side_to_move);
    Piece ally_piece = side2piece(_side_to_move, Piece::PAWN);
    uint64_t bb = _bitboards.get_bitboard(ally_piece);
    uint64_t not_occ = ~_bitboards.get_occupancy();
    uint64_t enemy_occ = _bitboards.get_per_side_occupancy(enemy_side);

    auto add_move = [&](Square from_sq, Square to_sq) {
        Piece captured_piece;
        if (BIT(to_sq) & enemy_occ) {
            captured_piece = _bitboards.piece_at(to_sq);
        } else {
            captured_piece = Piece::NONE;
        }

        Square rank_sq = rank_square(to_sq);
        if (rank_sq == rank_square(Square::A1) || rank_sq == rank_square(Square::A8)) {
            for (Piece promotion_piece = Piece::COLORLESS_IT_BEGIN; promotion_piece != Piece::COLORLESS_IT_END; next_colorless_piece(promotion_piece)) {
                if (promotion_piece == Piece::PAWN || promotion_piece == Piece::KING) {
                    continue;
                }

                Move &m = moves.emplace();
                m.from = from_sq;
                m.to = to_sq;

                m.piece = ally_piece;
                m.promotion_piece = side2piece(_side_to_move, promotion_piece);
                m.captured_piece = captured_piece;

                m.castling = Castling::NONE;
            }
        } else {
            Move &m = moves.emplace();
            m.from = from_sq;
            m.to = to_sq;

            m.piece = ally_piece;
            m.promotion_piece = Piece::NONE;
            m.captured_piece = captured_piece;

            m.castling = Castling::NONE;
        }
    };

    uint64_t en_passant_mask = _en_passant_target == Square::NONE ? 0 : BIT(_en_passant_target);

    while (bb) {
        Square from_sq = (Square)pop_idx_LSB(bb);
        uint64_t from_bb = BIT(from_sq);

        uint64_t moves_bb = _pawn_moves(_side_to_move, from_bb);
        moves_bb &= not_occ;
        uint64_t captures_bb = _pawn_captures(_side_to_move, from_sq);
        uint64_t captures_mask = enemy_occ;

        uint64_t check_mask_copy = _check_mask;
        if ((_en_passant_edge_case & from_bb) == 0) {
            captures_mask |= en_passant_mask;

            if (_check_mask & BIT(_en_passant_capture())) {
                check_mask_copy |= en_passant_mask;
            }
        }
        captures_bb &= enemy_occ | captures_mask;

        if (has_flag(BoardFlags::CHECK)) {
            moves_bb &= check_mask_copy;
            captures_bb &= check_mask_copy;
        }
        if (from_bb & _pinned_mask) {
            moves_bb &= _pinned_allowed_rays[(int)from_sq];
            captures_bb &= _pinned_allowed_rays[(int)from_sq];
        }

        while (moves_bb) {
            Square to_sq = (Square)pop_idx_LSB(moves_bb);
            add_move(from_sq, to_sq);
        }
        while (captures_bb) {
            Square to_sq = (Square)pop_idx_LSB(captures_bb);
            add_move(from_sq, to_sq);
        }
    }
}

void Board::_generate_basic_piece_moves(MoveContainer &moves, Piece piece, std::function<uint64_t(const Board*, Square)> moves_function) const {
    Side enemy_side = other_side(_side_to_move);
    Piece ally_piece = side2piece(_side_to_move, piece);
    uint64_t bb = _bitboards.get_bitboard(ally_piece);
    uint64_t not_occ = ~_bitboards.get_per_side_occupancy(_side_to_move);
    uint64_t enemy_occ = _bitboards.get_per_side_occupancy(enemy_side);

    while (bb) {
        Square from_sq = (Square)pop_idx_LSB(bb);
        uint64_t from_bb = BIT(from_sq);

        uint64_t moves_bb = moves_function(this, from_sq);
        moves_bb &= not_occ;

        if (has_flag(BoardFlags::CHECK)) {
            moves_bb &= _check_mask;
        }
        if (from_bb & _pinned_mask) {
            moves_bb &= _pinned_allowed_rays[(int)from_sq];
        }

        while (moves_bb) {
            Square to_sq = (Square)pop_idx_LSB(moves_bb);

            Piece captured_piece;
            if (BIT(to_sq) & enemy_occ) {
                captured_piece = _bitboards.piece_at(to_sq);
            } else {
                captured_piece = Piece::NONE;
            }

            Move &m = moves.emplace();
            m.from = from_sq;
            m.to = to_sq;

            m.piece = ally_piece;
            m.promotion_piece = Piece::NONE;
            m.captured_piece = captured_piece;

            m.castling = Castling::NONE;
        }
    }
}

void Board::_generate_king_moves(MoveContainer &moves) const {
    Side enemy_side = other_side(_side_to_move);
    Piece ally_king = side2piece(_side_to_move, Piece::KING);
    uint64_t king_bb = _bitboards.get_bitboard(ally_king);
    Square king_sq = (Square)scan_idx_LSB(king_bb);
    uint64_t enemy_occ = _bitboards.get_per_side_occupancy(enemy_side);

    uint64_t king_moves = _king_moves(king_sq);
    king_moves &= ~_attack_mask;
    king_moves &= ~_bitboards.get_per_side_occupancy(_side_to_move);

    while (king_moves) {
        Square to_sq = (Square)pop_idx_LSB(king_moves);

        Piece captured_piece;
        if (BIT(to_sq) & enemy_occ) {
            captured_piece = _bitboards.piece_at(to_sq);
        } else {
            captured_piece = Piece::NONE;
        }

        Move &m = moves.emplace();
        m.from = king_sq;
        m.to = to_sq;

        m.piece = ally_king;
        m.promotion_piece = Piece::NONE;
        m.captured_piece = captured_piece;

        m.castling = Castling::NONE;
    }

    if (!has_flag(BoardFlags::CHECK)) {
        uint64_t occ = _bitboards.get_occupancy();
        uint64_t both_mask = _attack_mask | occ;
        if (_side_to_move == Side::WHITE) {
            if (castling_allowed(Castling::WHITE_OO) && (white_oo_mask & both_mask) == 0) {
                Move &m = moves.emplace();
                m.from = king_sq;
                m.to = Square::G1;

                m.piece = ally_king;
                m.promotion_piece = Piece::NONE;
                m.captured_piece = Piece::NONE;

                m.castling = Castling::WHITE_OO;
            }

            if (castling_allowed(Castling::WHITE_OOO) &&
                (white_ooo_occ_mask & occ) == 0 && (white_ooo_attack_mask & _attack_mask) == 0
            ) {
                Move &m = moves.emplace();
                m.from = king_sq;
                m.to = Square::C1;

                m.piece = ally_king;
                m.promotion_piece = Piece::NONE;
                m.captured_piece = Piece::NONE;

                m.castling = Castling::WHITE_OOO;
            }
        }

        if (_side_to_move == Side::BLACK) {
            if (castling_allowed(Castling::BLACK_OO) && (black_oo_mask & both_mask) == 0) {
                Move &m = moves.emplace();
                m.from = king_sq;
                m.to = Square::G8;

                m.piece = ally_king;
                m.promotion_piece = Piece::NONE;
                m.captured_piece = Piece::NONE;

                m.castling = Castling::BLACK_OO;
            }

            if (castling_allowed(Castling::BLACK_OOO) &&
                (black_ooo_occ_mask & occ) == 0 && (black_ooo_attack_mask & _attack_mask) == 0
            ) {
                Move &m = moves.emplace();
                m.from = king_sq;
                m.to = Square::C8;

                m.piece = ally_king;
                m.promotion_piece = Piece::NONE;
                m.captured_piece = Piece::NONE;

                m.castling = Castling::BLACK_OOO;
            }
        }
    }
}

MoveContainer Board::generate_moves() const {
    MoveContainer moves;

    if (has_flag(BoardFlags::ENDED)) {
        return moves;
    }

    _generate_king_moves(moves);

    if (has_flag(BoardFlags::DOUBLE_CHECK)) {
        return moves;
    }

    _generate_basic_piece_moves(moves, Piece::KNIGHT, &Board::_knight_moves);
    _generate_basic_piece_moves(moves, Piece::BISHOP, &Board::_bishop_moves<false>);
    _generate_basic_piece_moves(moves, Piece::ROOK, &Board::_rook_moves<false>);
    _generate_basic_piece_moves(moves, Piece::QUEEN, &Board::_queen_moves<false>);
    _generate_pawn_moves(moves);

    return moves;
}

void Board::make_move(const Move &move) {
    if (move.captured_piece != Piece::NONE) {
        _bitboards.reset_piece(move.captured_piece, move.to);
    }

    _bitboards.reset_piece(move.piece, move.from);
    if (move.promotion_piece == Piece::NONE) {
        _bitboards.set_piece(move.piece, move.to);
    } else {
        _bitboards.set_piece(move.promotion_piece, move.to);
    }

    auto check_castling = [&](Castling castling, Side side, Square rook_from, Square rook_to) {
        if (move.castling == castling) {
            _bitboards.reset_piece(side2piece(side, Piece::ROOK), rook_from);
            _bitboards.set_piece(side2piece(side, Piece::ROOK), rook_to);
            _castling_state &= ~castling;
        }
    };

    check_castling(Castling::WHITE_OO, Side::WHITE, Square::H1, Square::F1);
    check_castling(Castling::WHITE_OOO, Side::WHITE, Square::A1, Square::D1);
    check_castling(Castling::BLACK_OO, Side::BLACK, Square::H8, Square::F8);
    check_castling(Castling::BLACK_OOO, Side::BLACK, Square::A8, Square::D8);

    if (move.piece == Piece::WHITE_KING) {
        _castling_state &= ~Castling::WHITE;
    } else if (move.piece == Piece::BLACK_KING) {
        _castling_state &= ~Castling::BLACK;
    }

    if (move.piece == Piece::WHITE_ROOK) {
        if (move.from == Square::A1) {
            _castling_state &= ~Castling::WHITE_OOO;
        } else if (move.from == Square::H1) {
            _castling_state &= ~Castling::WHITE_OO;
        }
    } else if (move.piece == Piece::BLACK_ROOK) {
        if (move.from == Square::A8) {
            _castling_state &= ~Castling::BLACK_OOO;
        } else if (move.from == Square::H8) {
            _castling_state &= ~Castling::BLACK_OO;
        }
    }

    if (move.captured_piece == Piece::WHITE_ROOK) {
        if (move.to == Square::A1) {
            _castling_state &= ~Castling::WHITE_OOO;
        } else if (move.to == Square::H1) {
            _castling_state &= ~Castling::WHITE_OO;
        }
    } else if (move.captured_piece == Piece::BLACK_ROOK) {
        if (move.to == Square::A8) {
            _castling_state &= ~Castling::BLACK_OOO;
        } else if (move.to == Square::H8) {
            _castling_state &= ~Castling::BLACK_OO;
        }
    }

    if (move.piece == side2piece(_side_to_move, Piece::PAWN)) {
        if (move.to == _en_passant_target) {
            _bitboards.reset_piece(piece_other_side(move.piece), _en_passant_capture());
        }

        int diff = (int)move.to - (int)move.from;
        if (std::abs(diff) == 16) {
            _en_passant_target = Square((int)move.from + diff / 2);
        } else {
            _en_passant_target = Square::NONE;
        }
    } else {
        _en_passant_target = Square::NONE;
    }

    _side_to_move = other_side(_side_to_move);

    _set_flags();
}

bool Board::make_move_from_string(const std::string &move) {
    if (move.size() != 4 && move.size() != 5) {
        return false;
    }

    Square from = str2square(move.substr(0, 2));
    Square to = str2square(move.substr(2, 2));

    Piece promotion_piece = Piece::NONE;
    if (move.size() == 5) {
        promotion_piece = side2piece(_side_to_move, str2piece(move[4]));
    }

    if (from == Square::NONE || to == Square::NONE) {
        return false;
    }

    MoveContainer legal_move_list = generate_moves();

    for (const Move &legal_move : legal_move_list) {
        if (legal_move.from == from && legal_move.to == to && legal_move.promotion_piece == promotion_piece) {
            make_move(legal_move);
            return true;
        }
    }

    return false;
}

Side Board::side_to_move() const {
    return _side_to_move;
}

Bitboards Board::get_bitboards() const {
    return _bitboards;
}

bool Board::castling_allowed(Castling c) const {
    return (_castling_state & c) != Castling::NONE;
}

bool Board::_import_fen_placement(const std::string &str) {
    Square sq = Square::IT_BEGIN;
    for (auto it = str.begin(); it != str.end(); it++) {
        char c = *it;
        if (c == '/') {
            continue;
        }

        if ('1' <= c && c <= '8') {
            sq = Square((int)sq + c - '0');
            continue;
        }

        auto piece = str2piece(c);
        if (piece != Piece::NONE) {
            _bitboards.set_piece(piece, sq);
            sq = Square((int)sq + 1);
            continue;
        }

        return false;
    }

    return true;
}

bool Board::_import_fen_side_to_move(const std::string &str) {
    if (str == "w") {
        _side_to_move = Side::WHITE;
        return true;
    } else if (str == "b") {
        _side_to_move = Side::BLACK;
        return true;
    }

    return false;
}

bool Board::_import_fen_castling(const std::string &str) {
    _castling_state = Castling::NONE;

    if (str.contains('K')) {
        _castling_state |= Castling::WHITE_OO;
    }
    if (str.contains('Q')) {
        _castling_state |= Castling::WHITE_OOO;
    }
    if (str.contains('k')) {
        _castling_state |= Castling::BLACK_OO;
    }
    if (str.contains('q')) {
        _castling_state |= Castling::BLACK_OOO;
    }

    return true;
}

bool Board::_import_fen_en_passant(const std::string &str) {
    if (str == "-" || str == " " || str == "") {
        _en_passant_target = Square::NONE;
        return true;
    }

    _en_passant_target = str2square(str);
    if (_en_passant_target == Square::NONE) {
        return false;
    }

    return true;
}

bool Board::_import_fen_clocks(const std::string &str1, const std::string &str2) {
    if (str1 == "-" || str1 == " " || str1 == "") {
        _half_move_clock = 0;
    } else {
        auto half_move_op = str2int32(str1);
        if (!half_move_op) {
            return false;
        }

        _half_move_clock = *half_move_op;
    }

    
    if (str2 == "-" || str2 == " " || str2 == "") {
        _full_move_count = 0;
    } else {
        auto full_move_op = str2int32(str2);
        if (!full_move_op) {
            return false;
        }

        _full_move_count = *full_move_op;
    }

    return true;
}

bool Board::import_fen(const std::string &fen) {
    _bitboards.clear();

    std::vector<std::string> parts = split_string(fen, ' ');
    if (parts.size() < 2) return false;

    const std::string dash = "-";
    if (!_import_fen_placement(parts[0]) ||
        !_import_fen_side_to_move(parts[1]) ||
        !_import_fen_castling(parts.size() >= 3 ? parts[2] : dash) ||
        !_import_fen_en_passant(parts.size() >= 4 ? parts[3] : dash) ||
        !_import_fen_clocks(parts.size() >= 5 ? parts[4] : dash, parts.size() >= 6 ? parts[5] : dash)
    ) {
        return false;
    }

    _set_flags();

    return true;
}

std::string Board::export_fen() const {
    return "";
}

Board::Board() {
    if (!import_fen(Board::s_starting_position)) {
        throw std::runtime_error("Failed to import starting position");
    }
}

}