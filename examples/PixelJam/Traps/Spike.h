//
// Created by nicol on 5/14/2025.
//

#ifndef SPIKE_H
#define SPIKE_H

namespace pg {
    /**
     * @brief Represents a spike trap that cycles through three states:
     *        In → Showing → Out → (loops).
     *
     * States:
     * - In:     Spikes are hidden; safe to walk on.
     * - Showing:Spikes are starting to emerge; visual warning only.
     * - Out:    Spikes are fully out; dangerous.
     */
    class Spike {
    public:
        /**
         * @brief The state of the spike trap.
         */
        enum class State {
            In,
            Showing,
            Out
        };

        /**
         * @brief Construct a Spike instance at a given position with timing settings.
         *
         *
         * @param inDur         Duration in seconds for the In state.
         * @param showDur       Duration in seconds for the Showing state.
         * @param outDur        Duration in seconds for the Out state.
         * @param offset        Optional cycle offset in seconds (used to desync multiple spikes).
         */
        Spike(float inDur, float showDur, float outDur, float offset = 0.0f)
            : inDuration(inDur), showingDuration(showDur), outDuration(outDur),
              state(State::In), timer(0.0f) {
            // Total duration of one full cycle
            float cycle = inDuration + showingDuration + outDuration;

            // Apply cycle offset
            float timeLeft = offset;

            while (timeLeft >= 0.0f) {
                switch (state) {
                    case State::In:
                        if (timeLeft < inDuration) {
                            timer = timeLeft;
                            return;
                        }
                        timeLeft -= inDuration;
                        state = State::Showing;
                        break;

                    case State::Showing:
                        if (timeLeft < showingDuration) {
                            timer = timeLeft;
                            return;
                        }
                        timeLeft -= showingDuration;
                        state = State::Out;
                        break;

                    case State::Out:
                        if (timeLeft < outDuration) {
                            timer = timeLeft;
                            return;
                        }
                        timeLeft -= outDuration;
                        state = State::In;
                        break;
                }
            }
        }

        /**
        * @brief Updates the spike state machine based on elapsed time.
        *
        * @param dt Delta time in seconds since the last update.
        */
        void update(float dt) {
            timer += dt;

            switch (state) {
                case State::In:
                    if (timer >= inDuration) {
                        timer -= inDuration;
                        state = State::Showing;
                    }
                    break;

                case State::Showing:
                    if (timer >= showingDuration) {
                        timer -= showingDuration;
                        state = State::Out;
                    }
                    break;

                case State::Out:
                    if (timer >= outDuration) {
                        timer -= outDuration;
                        state = State::In;
                    }
                    break;
            }
        }

        /**
     * @brief Gets the current state of the spike.
     * @return State enum (In, Showing, Out).
     */
        State getState() const { return state; }

    private:
        float inDuration;
        float showingDuration;
        float outDuration;

        float timer; // Time elapsed in the current state
        State state;
    };
};


#endif //SPIKE_H
