#include <memory>
#include <utility>

class AnimalProxy {
    struct Concept {
        virtual ~Concept() = default;
        virtual void say() const = 0;
    };

    template <typename T>
    struct Model : Concept {
        T data;

        template <typename U>
        Model(U&& u)
            : data(std::forward<U>(u))
        {
        }

        void say() const override
        {
            data.say();
        }
    };

    std::unique_ptr<Concept> self;

public:
    template <typename T>
    AnimalProxy(T&& x)
        : self(std::make_unique<Model<T>>(std::forward<T>(x)))
    {
    }

    void say() const
    {
        self->say();
    }
};
