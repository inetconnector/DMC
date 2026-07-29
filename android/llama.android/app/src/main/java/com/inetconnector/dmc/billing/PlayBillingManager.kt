package com.inetconnector.dmc.billing

import android.app.Activity
import android.content.Context
import com.android.billingclient.api.AcknowledgePurchaseParams
import com.android.billingclient.api.BillingClient
import com.android.billingclient.api.BillingClientStateListener
import com.android.billingclient.api.BillingFlowParams
import com.android.billingclient.api.BillingResult
import com.android.billingclient.api.PendingPurchasesParams
import com.android.billingclient.api.ProductDetails
import com.android.billingclient.api.Purchase
import com.android.billingclient.api.PurchasesUpdatedListener
import com.android.billingclient.api.QueryProductDetailsParams
import com.android.billingclient.api.QueryPurchasesParams
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlin.coroutines.resume
import kotlinx.coroutines.suspendCancellableCoroutine

data class BillingSnapshot(
    val unlocked: Boolean,
    val formattedPrice: String?,
    val available: Boolean,
    val message: String? = null
)

enum class PurchaseOutcome {
    PURCHASED,
    PENDING,
    CANCELLED,
    FAILED
}

class PlayBillingManager(
    context: Context,
    private val productId: String,
    private val onPurchaseOutcome: (PurchaseOutcome, String?) -> Unit
) : PurchasesUpdatedListener {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var productDetails: ProductDetails? = null

    private val billingClient = BillingClient.newBuilder(context.applicationContext)
        .setListener(this)
        .enablePendingPurchases(
            PendingPurchasesParams.newBuilder()
                .enableOneTimeProducts()
                .build()
        )
        .enableAutoServiceReconnection()
        .build()

    suspend fun querySnapshot(): BillingSnapshot {
        val connection = ensureConnected()
        if (connection.responseCode != BillingClient.BillingResponseCode.OK) {
            return BillingSnapshot(
                unlocked = false,
                formattedPrice = null,
                available = false,
                message = connection.debugMessage
            )
        }

        val purchases = queryPurchases()
        val owned = purchases.second.firstOrNull {
            it.purchaseState == Purchase.PurchaseState.PURCHASED && productId in it.products
        }
        if (owned != null) {
            val acknowledgement = acknowledgeIfNeeded(owned)
            if (acknowledgement.responseCode != BillingClient.BillingResponseCode.OK) {
                return BillingSnapshot(
                    unlocked = false,
                    formattedPrice = null,
                    available = true,
                    message = acknowledgement.debugMessage
                )
            }
            return BillingSnapshot(unlocked = true, formattedPrice = null, available = true)
        }

        val details = queryProduct()
        productDetails = details
        return BillingSnapshot(
            unlocked = false,
            formattedPrice = details?.oneTimePurchaseOfferDetails?.formattedPrice,
            available = details != null,
            message = if (details == null) purchases.first.debugMessage else null
        )
    }

    fun launchPurchase(activity: Activity): BillingResult {
        val details = productDetails
            ?: return BillingResult.newBuilder()
                .setResponseCode(BillingClient.BillingResponseCode.ITEM_UNAVAILABLE)
                .setDebugMessage("Play product details are not available.")
                .build()
        val productParams = BillingFlowParams.ProductDetailsParams.newBuilder()
            .setProductDetails(details)
            .build()
        val params = BillingFlowParams.newBuilder()
            .setProductDetailsParamsList(listOf(productParams))
            .setIsOfferPersonalized(false)
            .build()
        return billingClient.launchBillingFlow(activity, params)
    }

    override fun onPurchasesUpdated(
        billingResult: BillingResult,
        purchases: MutableList<Purchase>?
    ) {
        when (billingResult.responseCode) {
            BillingClient.BillingResponseCode.OK -> {
                val purchase = purchases.orEmpty().firstOrNull { productId in it.products }
                when (purchase?.purchaseState) {
                    Purchase.PurchaseState.PURCHASED -> scope.launch {
                        val acknowledgement = acknowledgeIfNeeded(purchase)
                        if (acknowledgement.responseCode ==
                            BillingClient.BillingResponseCode.OK
                        ) {
                            onPurchaseOutcome(PurchaseOutcome.PURCHASED, null)
                        } else {
                            onPurchaseOutcome(
                                PurchaseOutcome.FAILED,
                                acknowledgement.debugMessage
                            )
                        }
                    }
                    Purchase.PurchaseState.PENDING ->
                        onPurchaseOutcome(PurchaseOutcome.PENDING, billingResult.debugMessage)
                    else -> onPurchaseOutcome(PurchaseOutcome.FAILED, billingResult.debugMessage)
                }
            }
            BillingClient.BillingResponseCode.USER_CANCELED ->
                onPurchaseOutcome(PurchaseOutcome.CANCELLED, null)
            else -> onPurchaseOutcome(PurchaseOutcome.FAILED, billingResult.debugMessage)
        }
    }

    fun close() {
        scope.cancel()
        billingClient.endConnection()
    }

    private suspend fun ensureConnected(): BillingResult {
        if (billingClient.isReady) {
            return BillingResult.newBuilder()
                .setResponseCode(BillingClient.BillingResponseCode.OK)
                .build()
        }
        return suspendCancellableCoroutine { continuation ->
            billingClient.startConnection(object : BillingClientStateListener {
                override fun onBillingSetupFinished(result: BillingResult) {
                    if (continuation.isActive) continuation.resume(result)
                }

                override fun onBillingServiceDisconnected() {
                    // Auto-reconnection is enabled. The active request receives its setup result.
                }
            })
        }
    }

    private suspend fun queryPurchases(): Pair<BillingResult, List<Purchase>> =
        suspendCancellableCoroutine { continuation ->
            val params = QueryPurchasesParams.newBuilder()
                .setProductType(BillingClient.ProductType.INAPP)
                .build()
            billingClient.queryPurchasesAsync(params) { result, purchases ->
                if (continuation.isActive) continuation.resume(result to purchases)
            }
        }

    private suspend fun queryProduct(): ProductDetails? =
        suspendCancellableCoroutine { continuation ->
            val product = QueryProductDetailsParams.Product.newBuilder()
                .setProductId(productId)
                .setProductType(BillingClient.ProductType.INAPP)
                .build()
            val params = QueryProductDetailsParams.newBuilder()
                .setProductList(listOf(product))
                .build()
            billingClient.queryProductDetailsAsync(params) { result, detailsResult ->
                val details = if (result.responseCode == BillingClient.BillingResponseCode.OK) {
                    detailsResult.productDetailsList.firstOrNull()
                } else {
                    null
                }
                if (continuation.isActive) continuation.resume(details)
            }
        }

    private suspend fun acknowledgeIfNeeded(purchase: Purchase): BillingResult {
        if (purchase.isAcknowledged) {
            return BillingResult.newBuilder()
                .setResponseCode(BillingClient.BillingResponseCode.OK)
                .build()
        }
        return suspendCancellableCoroutine { continuation ->
            val params = AcknowledgePurchaseParams.newBuilder()
                .setPurchaseToken(purchase.purchaseToken)
                .build()
            billingClient.acknowledgePurchase(params) { result ->
                if (continuation.isActive) continuation.resume(result)
            }
        }
    }
}
