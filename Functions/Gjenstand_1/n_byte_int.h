#ifndef N_BIT_BYTE_H
#define N_BIT_BYTE_H 

#include <cstddef>
#include <array>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <bit>

// Implementation details for N_byte_int. Some of these functions may be useful elsewhere,
// and may want to be moved to their own header/namespace.
namespace n_byte_int_detail {

// Returns a bidirectional iterator that iterates over the byte representation of Scalar, 
// starting at the least significant byte and ending the most significant byte.
template <typename Scalar,
	std::enable_if_t<std::is_scalar_v<Scalar>, bool> = true
>
[[nodiscard]]
auto little_endian_byte_iterator(Scalar& scalar) noexcept {
	if constexpr (std::endian::native == std::endian::little) {
		return reinterpret_cast<std::byte*>(&scalar);
	}
	else {
		return std::make_reverse_iterator(reinterpret_cast<std::byte*>(&scalar));
	}
}

// Returns the end iterator for the byte representation of Scalar.
template <typename Scalar,
	std::enable_if_t<std::is_scalar_v<Scalar>, bool> = true
>
[[nodiscard]]
auto little_endian_byte_end_iterator(Scalar& scalar) noexcept {
	return std::next(little_endian_byte_iterator(scalar), sizeof(Scalar));
}

// Returns a bidirectional const iterator that iterates over the byte representation of Scalar, 
// starting at the least significant byte and ending the most significant byte.
template <typename Scalar,
	std::enable_if_t<std::is_scalar_v<Scalar>, bool> = true
>
[[nodiscard]]
auto little_endian_byte_iterator(const Scalar& scalar) noexcept {
	if constexpr (std::endian::native == std::endian::little) {
		return reinterpret_cast<const std::byte*>(&scalar);
	}
	else {
		return std::make_reverse_iterator(reinterpret_cast<const std::byte*>(&scalar));
	}
}

// Returns the end iterator for the byte representation of Scalar.
template <typename Scalar,
	std::enable_if_t<std::is_scalar_v<Scalar>, bool> = true
>
[[nodiscard]]
auto little_endian_byte_end_iterator(const Scalar& scalar) noexcept {
	return std::next(little_endian_byte_iterator(scalar), sizeof(Scalar));
}

// Manually converts a signed integer to its two's complement representation. 
// For versions prior to C++20, the C++ standard does not guarantee that signed integers are
// implemented as two's complement, so this function ensures that we are compliant and portable 
// in C++17.
template <typename Sint,
	std::enable_if_t<std::is_signed_v<Sint>, bool> = true
>
[[nodiscard]] constexpr
auto to_twos_complement(Sint value) noexcept -> std::make_unsigned_t<Sint> {
	if (value >= 0) {
		return static_cast<std::make_unsigned_t<Sint>>(value);
	}
	else {
		return ~static_cast<std::make_unsigned_t<Sint>>(value * -1) + 1;
	}
}

// Manually interprets an unsigned itneger as a signed integer represented by two's complement. 
// For versions prior to C++20, the C++ standard does not guarantee that signed integers are
// implemented as two's complement, so this function ensures that we are compliant and portable 
// in C++17.
template <typename Uint,
	std::enable_if_t<std::is_unsigned_v<Uint>, bool> = true
>
[[nodiscard]] constexpr
auto from_twos_complement(Uint value) noexcept -> std::make_signed_t<Uint> {
	if ((value & ((Uint{1} << (sizeof(Uint) * CHAR_BIT - 1)))) == 0) {
		return static_cast<std::make_signed_t<Uint>>(value);
	}
	else {
		return static_cast<std::make_signed_t<Uint>>(~value + 1) * -1;
	}
}

} // namespace n_byte_int_detail

// A type taking up exactly N bytes in memory, which can more or less be used like an integer.
template <
	typename Int, 
	std::size_t Num_bytes,
	std::endian Byte_order = std::endian::native,
	std::enable_if_t<
		(std::is_integral_v<Int> && Num_bytes <= sizeof(Int)), 
		bool
	> = true
>
class N_byte_int{
	static_assert(
		(std::endian::native == std::endian::big) || (std::endian::native == std::endian::little), 
		"Mixed-endian platforms are not supported"
	);
	static_assert(
		(Byte_order == std::endian::big) || (Byte_order == std::endian::little), 
		"The byte order myst be little or big endian"
	);

	std::array<std::byte, Num_bytes> data_;

	[[nodiscard]] constexpr
	auto data_begin_little_endian() noexcept {
		if constexpr (Byte_order == std::endian::little) {
			return data_.begin();
		}
		else {
			return data_.rbegin();
		}
	}

	[[nodiscard]] constexpr
	auto data_end_little_endian() noexcept {
		if constexpr (Byte_order == std::endian::little) {
			return data_.end();
		}
		else {
			return data_.rend();
		}
	}

	[[nodiscard]] constexpr
	auto data_begin_little_endian() const noexcept {
		if constexpr (Byte_order == std::endian::little) {
			return data_.cbegin();
		}
		else {
			return data_.crbegin();
		}
	}

	[[nodiscard]] constexpr
	auto data_end_little_endian() const noexcept {
		if constexpr (Byte_order == std::endian::little) {
			return data_.cend();
		}
		else {
			return data_.crend();
		}
	}

	[[nodiscard]] constexpr
	auto data_msb() const noexcept -> const std::byte& {
		return *std::prev(data_end_little_endian());
	}

	[[nodiscard]] constexpr
	auto data_msb() noexcept -> std::byte {
		return *std::prev(data_end_little_endian());
	}

	void assign_value(Int value) noexcept {
		const std::make_unsigned_t<Int> uvalue = [&] {
			if constexpr (std::is_signed_v<Int>) {
				return to_twos_complement(value);
			}
			else {
				return static_cast<std::make_unsigned_t<Int>>(value);
			}
		}();
		std::copy_n(little_endian_byte_iterator(uvalue), data_.size(), data_begin_little_endian());
	}

	auto get_value() const noexcept {
		std::make_unsigned_t<Int> uvalue{};
		if constexpr (std::is_signed_v<Int> && Num_bytes < sizeof(Int)) {
			if ((data_msb() & std::byte{0x80}) != std::byte{0}) {
				uvalue = ~uvalue;
			}
		}
		
		std::copy_n(data_begin_little_endian(), data_.size(), little_endian_byte_iterator(uvalue));
		
		if constexpr (std::is_signed_v<Int>) {
			return from_twos_complement(uvalue);
		}
		else {
			return static_cast<Int>(uvalue);
		}
	}
    
public:
	/*implicit*/ N_byte_int(Int value = {}) noexcept {
		assign_value(value);
	}

	auto operator=(Int value) noexcept -> N_byte_int& {
		assign_value(value);
		return *this;
	}

	/*implicit*/ operator Int() const {
		return get_value();
	}

	[[nodiscard]]
	auto value() const noexcept -> Int {
		return get_value();
	}

	[[nodiscard]] constexpr
	auto bytes() const noexcept -> const std::array<std::byte, Num_bytes>& {
		return data_;
	}
};

#endif