#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>

#include <SFML/Graphics.hpp>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
const int FPS_LIMIT = 30;
const int FRAMES_PER_SEGMENT = 90;  // frames for the square to cross one segment
const float SQUARE_SIZE = 20.f;
int frameCount = 0;

const int CURVE_SAMPLES = 60;    // line segments used to draw each Bezier segment
const float POINT_RADIUS = 7.f;  // radius of control point circles
const float EDGE_MARGIN = 20.f;  // keep new points this far from the window edge

using Point2D = sf::Vector2f;

// Sample a cubic Bezier curve at t in [0, 1] using the first four points in pts.
//   B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3
Point2D getPoint(const std::vector<sf::Vector2f>& pts, float t) {
    float u = 1.f - t;
    float b0 = u * u * u;
    float b1 = 3.f * u * u * t;
    float b2 = 3.f * u * t * t;
    float b3 = t * t * t;
    return b0 * pts[0] + b1 * pts[1] + b2 * pts[2] + b3 * pts[3];
}

// Derivative of the curve at t (uses the first four points in pts).
Point2D getSlope(const std::vector<sf::Vector2f>& pts, float t) {
    float u = 1.f - t;
    Point2D term0 = 3.f * u * u * (pts[1] - pts[0]);
    Point2D term1 = 6.f * u * t * (pts[2] - pts[1]);
    Point2D term2 = 3.f * t * t * (pts[3] - pts[2]);
    return term0 + term1 + term2;
}

// Control points. Segment s uses points[3s] .. points[3s+3]; neighboring
// segments share an endpoint (the "joint").
std::vector<sf::Vector2f> points = {
    {100.f, 600.f},
    {200.f, 200.f},
    {600.f, 200.f},
    {700.f, 600.f},
};

// Index of the control point being dragged, or -1 if none.
int selectedIndex = -1;

int numSegments() { return static_cast<int>((points.size() - 1) / 3); }

// Copy the four control points of segment s into their own vector so
// getPoint / getSlope can be used unchanged.
std::vector<sf::Vector2f> segmentPoints(int s) {
    return {points[3 * s], points[3 * s + 1], points[3 * s + 2], points[3 * s + 3]};
}

// Draw a line between two points.
// (Swap this body for the line-drawing code from your project if you want to reuse it.)
void drawLine(sf::RenderWindow& window, Point2D a, Point2D b, sf::Color color) {
    sf::Vertex line[] = {
        {a, color},
        {b, color},
    };
    window.draw(line, 2, sf::PrimitiveType::Lines);
}

Point2D clampToWindow(Point2D p) {
    p.x = std::clamp(p.x, EDGE_MARGIN, WINDOW_WIDTH - EDGE_MARGIN);
    p.y = std::clamp(p.y, EDGE_MARGIN, WINDOW_HEIGHT - EDGE_MARGIN);
    return p;
}

// '+': add three points (one more Bezier segment) to the end of the curve.
void addSegment() {
    Point2D last = points.back();
    Point2D prev = points[points.size() - 2];

    // First new point continues the previous handle's direction => smooth joint.
    Point2D p1 = clampToWindow(last + (last - prev));

    // Extend to the right unless we're near the right edge, then extend left.
    float dx = (last.x + 150.f < WINDOW_WIDTH - EDGE_MARGIN) ? 150.f : -150.f;
    Point2D p3 = clampToWindow(last + Point2D{dx, 0.f});

    // Bow the middle handle up or down (alternating) so the curve isn't flat.
    float bow = (numSegments() % 2 == 0) ? -100.f : 100.f;
    Point2D p2 = clampToWindow((p1 + p3) / 2.f + Point2D{0.f, bow});

    points.push_back(p1);
    points.push_back(p2);
    points.push_back(p3);
}

// '-': remove three points, but never go below four.
void removeSegment() {
    if (points.size() <= 4) {
        return;
    }
    points.resize(points.size() - 3);
    if (selectedIndex >= static_cast<int>(points.size())) {
        selectedIndex = -1;
    }
}

// Move the selected point to newPos, keeping the curve smooth at joints.
void dragPoint(int i, Point2D newPos) {
    int n = static_cast<int>(points.size());

    if (i % 3 == 0) {
        // Dragging a joint (or the very first/last point): carry its handles along.
        Point2D delta = newPos - points[i];
        points[i] = newPos;
        if (i > 0) points[i - 1] += delta;
        if (i + 1 < n) points[i + 1] += delta;
        return;
    }

    // Dragging a handle. If it's next to a joint that has a handle on the other
    // side, that partner handle must stay on the same line through the joint,
    // at its original distance.
    int joint = -1;
    int partner = -1;
    if (i % 3 == 2 && i + 2 < n) {  // handle before a joint, partner after it
        joint = i + 1;
        partner = i + 2;
    } else if (i % 3 == 1 && i >= 4) {  // handle after a joint, partner before it
        joint = i - 1;
        partner = i - 2;
    }

    if (joint == -1) {
        points[i] = newPos;
        return;
    }

    float partnerDist = (points[partner] - points[joint]).length();  // measure first
    points[i] = newPos;
    Point2D dir = points[joint] - points[i];  // from dragged handle toward joint
    if (dir.lengthSquared() > 1e-6f) {
        points[partner] = points[joint] + dir.normalized() * partnerDist;
    }
}

void handleInput(sf::Window& window, bool& shouldQuit) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            shouldQuit = true;
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouse->button == sf::Mouse::Button::Left) {
                sf::Vector2f click(mouse->position);
                float bestDist = (click - points[0]).lengthSquared();
                selectedIndex = 0;
                for (size_t i = 1; i < points.size(); i++) {
                    float d = (click - points[i]).lengthSquared();
                    if (d < bestDist) {
                        bestDist = d;
                        selectedIndex = static_cast<int>(i);
                    }
                }
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouse->button == sf::Mouse::Button::Left) {
                selectedIndex = -1;
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
            if (selectedIndex != -1) {
                dragPoint(selectedIndex, sf::Vector2f(mouse->position));
            }
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            // '+' is Shift+'=' on most keyboards, so accept '=' too; also the numpad keys.
            if (key->code == sf::Keyboard::Key::Equal || key->code == sf::Keyboard::Key::Add) {
                addSegment();
            } else if (key->code == sf::Keyboard::Key::Hyphen ||
                       key->code == sf::Keyboard::Key::Subtract) {
                removeSegment();
            }
        }
    }
}

void render(sf::RenderWindow& window) {
    window.clear(sf::Color::Black);

    int segs = numSegments();

    // Curve and control handles for every segment.
    for (int s = 0; s < segs; s++) {
        std::vector<sf::Vector2f> seg = segmentPoints(s);

        Point2D prev = getPoint(seg, 0.f);
        for (int i = 1; i <= CURVE_SAMPLES; i++) {
            float t = static_cast<float>(i) / CURVE_SAMPLES;
            Point2D cur = getPoint(seg, t);
            drawLine(window, prev, cur, sf::Color::Cyan);
            prev = cur;
        }

        // Handles: first->second and third->fourth point of this segment.
        drawLine(window, seg[0], seg[1], sf::Color(200, 80, 80));
        drawLine(window, seg[2], seg[3], sf::Color(200, 80, 80));
    }

    // Control points on top: joints in yellow, handles in white.
    for (size_t i = 0; i < points.size(); i++) {
        sf::CircleShape circle(POINT_RADIUS);
        circle.setOrigin({POINT_RADIUS, POINT_RADIUS});
        circle.setPosition(points[i]);
        circle.setFillColor(i % 3 == 0 ? sf::Color::Yellow : sf::Color::White);
        window.draw(circle);
    }

    // Square travelling along the whole path.
    int loopFrames = FRAMES_PER_SEGMENT * segs;
    float pathT = static_cast<float>(frameCount % loopFrames) / loopFrames * segs;
    int s = std::min(static_cast<int>(pathT), segs - 1);
    float localT = pathT - static_cast<float>(s);

    std::vector<sf::Vector2f> seg = segmentPoints(s);
    Point2D pos = getPoint(seg, localT);
    Point2D slope = getSlope(seg, localT);

    sf::RectangleShape square({SQUARE_SIZE * 1.5f, SQUARE_SIZE});
    square.setOrigin(square.getSize() / 2.f);
    square.setPosition(pos);
    square.setRotation(sf::radians(std::atan2(slope.y, slope.x)));
    square.setFillColor(sf::Color::Green);
    window.draw(square);

    // TODO: (Bonus) Support multiple curves, a Galaga screen overlay at a 1:2 ratio, and
    // exporting curve points as C++ code for Project 1b.

    frameCount++;
    window.display();
}

int main() {
    sf::RenderWindow window;

    try {
        window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Bezier Curve Editor");
        window.setFramerateLimit(FPS_LIMIT);
        window.setKeyRepeatEnabled(false);

        bool shouldQuit = false;
        while (window.isOpen()) {
            handleInput(window, shouldQuit);
            if (shouldQuit) {
                break;
            }
            render(window);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}