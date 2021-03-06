#pragma once

#include "RE/hkArray.h"
#include "RE/hkMultiThreadCheck.h"
#include "RE/hkpLinkedCollidable.h"
#include "RE/hkpProperty.h"
#include "RE/hkReferencedObject.h"
#include "RE/hkStringPtr.h"


namespace RE
{
	class hkMotionState;
	class hkpCollidable;
	class hkpShapeModifier;
	class hkpWorld;


	namespace hkWorldOperation
	{
		enum class Result : UInt32
		{
			kPostponed,
			kDone
		};
	}


	class hkpWorldObject : public hkReferencedObject
	{
	public:
		inline static constexpr auto RTTI = RTTI_hkpWorldObject;


		enum class MultiThreadingChecks : UInt32
		{
			kEnable,
			kIgnore
		};


		enum class BroadPhaseType : UInt32
		{
			kInvalid,
			kEntity,
			kPhantom,
			kPhaseBorder,

			kTotal
		};


		virtual ~hkpWorldObject();																												// 00

		// override (hkReferencedObject)
		virtual void						CalcContentStatistics(hkStatisticsCollector* a_collector, const hkClass* a_class) const override;	// 02

		// add
		virtual hkWorldOperation::Result	SetShape(const hkpShape* a_shape);																	// 03 - { return hkWorldOperation::Result::kDone; }
		virtual hkWorldOperation::Result	UpdateShape(hkpShapeModifier* a_shapeModifier);														// 04 - { return hkWorldOperation::Result::kDone; }
		virtual hkMotionState*				GetMotionState() = 0;																				// 05

		const hkpCollidable*	GetCollidable() const;
		hkpCollidable*			GetCollidableRW();


		// members
		hkpWorld*				world;				// 10
		UInt64					userData;			// 18 - bhkWorldObject*?
		hkpLinkedCollidable		collidable;			// 20
		hkMultiThreadCheck		multiThreadCheck;	// A0
		UInt32					padAC;				// AC
		hkStringPtr				name;				// B0
		hkArray<hkpProperty>	properties;			// B8
		void*					treeData;			// C8
	};
	STATIC_ASSERT(sizeof(hkpWorldObject) == 0xD0);
}
