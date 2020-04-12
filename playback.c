static const float NOTE_RATE_TABLE[12] = {
    1.0,
    1.0594630943593,
    1.12246204830937,
    1.18920711500272,
    1.25992104989487,
    1.33483985417003,
    1.4142135623731,
    1.49830707687668,
    1.5874010519682,
    1.68179283050743,
    1.78179743628068,
    1.88774862536339,
};

float note_rate(int note) {
    int key = note % 12;
    int oct = note / 12;
    float rate = NOTE_RATE_TABLE[key];
    if (oct > 5)
        return rate * (1 << (oct - 5));
    else if (oct < 5)
        return rate / (1 << (5 - oct));
    else
        return rate;
}