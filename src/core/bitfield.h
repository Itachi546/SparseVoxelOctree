template <typename T>
class BitField {
    int64_t value = 0;

  public:
    inline BitField<T> &set_flag(T flag) {
        value |= (int64_t)flag;
        return *this;
    }
    inline constexpr BitField() = default;
    inline constexpr BitField(int64_t value) { this->value = value; }
    inline constexpr BitField(T value) { this->value = (int64_t)value; }
    inline operator int64_t() const { return value; }
};